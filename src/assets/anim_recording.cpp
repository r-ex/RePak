#include "pch.h"
#include "assets.h"
#include "public/anim_recording.h"

static void AnimRecording_ParseFromANIR(const char* const assetPath, BinaryIO& bio, AnimRecordingFileHeader_s& hdr, size_t& totalBufSize)
{
	if (!bio.Open(assetPath, BinaryIO::Mode_e::Read))
		Error("Failed to open animation recording file \"%s\".\n", assetPath);

	const size_t fileSize = bio.GetSize();

	if (bio.GetSize() <= sizeof(AnimRecordingFileHeader_s))
		Error("Animation recording file \"%s\" appears truncated (%zu <= %zu).\n", assetPath, fileSize, sizeof(AnimRecordingFileHeader_s));

	bio.Read(hdr);

	if (hdr.magic != ANIR_FILE_MAGIC)
		Error("Attempted to load an invalid animation recording file (expected magic %x, got %x).\n", ANIR_FILE_MAGIC, hdr.magic);

	if (hdr.fileVersion != ANIR_FILE_VERSION)
		Error("Attempted to load an unsupported animation recording file (expected file version %x, got %x).\n", ANIR_FILE_VERSION, hdr.fileVersion);

	if (hdr.assetVersion != ANIR_VERSION)
		Error("Attempted to load an unsupported animation recording file (expected asset version %x, got %x).\n", ANIR_VERSION, hdr.assetVersion);

	if (hdr.numElements > ANIR_MAX_ELEMENTS)
		Error("Animation recording file \"%s\" has too many elements (max %d, got %d).\n", assetPath, ANIR_MAX_ELEMENTS, hdr.numElements);

	if (hdr.numSequences > ANIR_MAX_SEQUENCES)
		Error("Animation recording file \"%s\" has too many sequences (max %d, got %d).\n", assetPath, ANIR_MAX_SEQUENCES, hdr.numSequences);

	if (hdr.numRecordedFrames == 0)
		Error("Animation recording file \"%s\" has 0 frames.\n", assetPath);

	if (hdr.numRecordedFrames > ANIR_MAX_RECORDED_FRAMES)
		Error("Animation recording file \"%s\" has too many frames (max %d, got %d).\n", assetPath, ANIR_MAX_RECORDED_FRAMES, hdr.numRecordedFrames);

	// NOTE: the overlay count can be 0, so only check for max here, which is equal to frames.
	if (hdr.numRecordedOverlays > ANIR_MAX_RECORDED_FRAMES)
		Error("Animation recording file \"%s\" has too many overlays (max %d, got %d).\n", assetPath, ANIR_MAX_RECORDED_FRAMES, hdr.numRecordedOverlays);

	const size_t stringBufSize = IALIGN4(hdr.stringBufSize);

	const size_t animFramesBufSize = hdr.numRecordedFrames * sizeof(AnimRecordingFrame_s);
	const size_t animOverlaysBufSize = hdr.numRecordedOverlays * sizeof(AnimRecordingOverlay_s);

	totalBufSize = stringBufSize + animFramesBufSize + animOverlaysBufSize;
}

// page chunk structure and order:
// - header HEAD        (align=8)
// - data   CPU         (align=4)
static void AnimRecording_InternalAddAnimRecording(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath)
{
	const std::string anirPath = Utils::ChangeExtension(pak->GetAssetPath() + assetPath, "anir");

	BinaryIO bio;
	AnimRecordingFileHeader_s fileHdr; size_t cpuBufSize;

	AnimRecording_ParseFromANIR(anirPath.c_str(), bio, fileHdr, cpuBufSize);

	PakAsset_t& asset = pak->BeginAsset(assetGuid, assetPath);

	PakPageLump_s hdrLump = pak->CreatePageLump(sizeof(AnimRecordingAssetHeader_s), SF_HEAD | SF_SERVER, 8);
	AnimRecordingAssetHeader_s* const pHdr = reinterpret_cast<AnimRecordingAssetHeader_s*>(hdrLump.data);

	pHdr->startPos = fileHdr.startPos;
	pHdr->startAngles = fileHdr.startAngles;

	pHdr->numRecordedFrames = fileHdr.numRecordedFrames;
	pHdr->numRecordedOverlays = fileHdr.numRecordedOverlays;

	// Only used by the runtime when these assets get recorded,
	// needs to be -1 in the pak file.
	pHdr->runtimeSlotIndex = -1;
	pHdr->runtimeSlotIndexSign = -1;

	pHdr->animRecordingId = fileHdr.animRecordingId;

	// Anim recordings inside pak files must be marked as persistent,
	// else code will try to free it.
	pHdr->isPersistent = true;
	pHdr->runtimeRefCounter = 0;

	size_t cpuBufIt = 0;
	PakPageLump_s cpuLump = pak->CreatePageLump(cpuBufSize, SF_CPU | SF_SERVER, 4);

	for (int i = 0; i < fileHdr.numElements; i++)
	{
		std::string poseParamName;

		if (!bio.ReadString(poseParamName))
			Error("");

		const size_t stringBufLen = poseParamName.length() + 1;
		memcpy(&cpuLump.data[cpuBufIt], poseParamName.c_str(), stringBufLen);

		pak->AddPointer(hdrLump, offsetof(AnimRecordingAssetHeader_s, poseParamNames) + i * sizeof(PagePtr_t), cpuLump, cpuBufIt);
		cpuBufIt += stringBufLen;
	}

	for (int i = 0; i < fileHdr.numElements; i++)
	{
		bio.Read(pHdr->poseParamValues[i]);
	}

	for (int i = 0; i < fileHdr.numSequences; i++)
	{
		std::string sequenceName;

		if (!bio.ReadString(sequenceName))
			Error("");

		const size_t stringBufLen = sequenceName.length() + 1;
		memcpy(&cpuLump.data[cpuBufIt], sequenceName.c_str(), stringBufLen);

		pak->AddPointer(hdrLump, offsetof(AnimRecordingAssetHeader_s, animSequences) + i * sizeof(PagePtr_t), cpuLump, cpuBufIt);
		cpuBufIt += stringBufLen;
	}

	// Now the frames and overlays are getting written out, these must be aligned
	// to 4 bytes, so align the current buffer iterator out. The extra size taken
	// by this alignment is being accounted for in AnimRecording_ParseFromANIR().
	cpuBufIt = IALIGN4(cpuBufIt);
	pak->AddPointer(hdrLump, offsetof(AnimRecordingAssetHeader_s, recordedFrames), cpuLump, cpuBufIt);

	for (int i = 0; i < fileHdr.numRecordedFrames; i++)
	{
		bio.Read(&cpuLump.data[cpuBufIt], sizeof(AnimRecordingFrame_s));
		cpuBufIt += sizeof(AnimRecordingFrame_s);
	}

	if (fileHdr.numRecordedOverlays > 0)
	{
		pak->AddPointer(hdrLump, offsetof(AnimRecordingAssetHeader_s, recordedOverlays), cpuLump, cpuBufIt);

		for (int i = 0; i < fileHdr.numRecordedOverlays; i++)
		{
			bio.Read(&cpuLump.data[cpuBufIt], sizeof(AnimRecordingOverlay_s));
			cpuBufIt += sizeof(AnimRecordingOverlay_s);
		}
	}

	asset.InitAsset(hdrLump.GetPointer(), sizeof(AnimRecordingAssetHeader_s), PagePtr_t::NullPtr(), ANIR_VERSION, AssetType::ANIR);
	asset.SetHeaderPointer(hdrLump.data);

	pak->FinishAsset();
}

void Assets::AddAnimRecording_v1(CPakFileBuilder* const pak, const PakGuid_t assetGuid, const char* const assetPath, const rapidjson::Value& /*mapEntry*/)
{
	AnimRecording_InternalAddAnimRecording(pak, assetGuid, assetPath);
}
