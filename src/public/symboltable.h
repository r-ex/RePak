#pragma once
#include "utils/logger.h"

class CStringPool
{
public:
	CStringPool() = default;

	const char* Add(std::string_view str)
	{
		if (str.empty())
			return "";

		m_pool.emplace_back(str);
		m_totalStringBytes += str.size() +1; // +1 for null terminator.

		return m_pool.back().c_str();
	}

	void AppendRetainedStringSize(const size_t len)
	{
		m_numUnstaleStrings++;
		assert(m_numUnstaleStrings <= m_pool.size());
		m_retainedStringBytes += len +1;
		assert(m_retainedStringBytes <= m_totalStringBytes);
	}

	inline size_t StringBytesTotal() const noexcept { return m_totalStringBytes; }
	inline size_t StringBytesDiscarded() const noexcept { return m_totalStringBytes - m_retainedStringBytes; }
	inline size_t StringBytesRetained() const noexcept { return m_retainedStringBytes; }

	inline size_t NumStringsTotal() const noexcept { return m_pool.size(); }
	inline size_t NumStringsDiscarded() const noexcept { return m_pool.size() - m_retainedStringBytes; }
	inline size_t NumStringsRetained() const noexcept { return m_retainedStringBytes; }

private:
	std::deque<std::string> m_pool;
	size_t m_numUnstaleStrings = 0;
	size_t m_totalStringBytes = 0;
	size_t m_retainedStringBytes = 0;
};

template <bool CaseInsensitive = false>
class CSymbolTable
{
public:
	using SymbolId_t = uint32_t;
	static constexpr SymbolId_t npos = (std::numeric_limits<SymbolId_t>::max)();

	explicit CSymbolTable(const size_t poolReserve = 1024)
	{
		m_symbols.reserve(poolReserve);
		if constexpr (!CaseInsensitive)
			m_lookupSensitive.reserve(poolReserve);
		else
			m_lookupInsensitive.reserve(poolReserve);
	}

	SymbolId_t AddString(const std::string_view& str)
	{
		auto& lookup = getLookup();
		if (auto it = lookup.find(str); it != lookup.end())
			return it->second;

		if (m_symbols.size() >= static_cast<size_t>(npos))
			Error("CSymbolTable: SymbolId overflow");

		const char* stored = m_pool.Add(str);
		SymbolId_t id = static_cast<SymbolId_t>(m_symbols.size());
		m_symbols.emplace_back(stored, str.size());
		lookup.emplace(m_symbols.back(), id);
		return id;
	}

	std::string_view GetString(const SymbolId_t id) const
	{
		if (id == npos || static_cast<size_t>(id) >= m_symbols.size())
			Error("CSymbolTable::GetString: invalid ID");

		return m_symbols[id];
	}

	SymbolId_t Find(const std::string_view& str) const noexcept
	{
		const auto& lookup = getLookup();
		if (auto it = lookup.find(str); it != lookup.end())
			return it->second;
		return npos;
	}

	void MarkAsDiscarded(const SymbolId_t id) noexcept
	{
		if (id == npos || static_cast<size_t>(id) >= m_symbols.size())
			return;

		if (m_discardSet.find(id) != m_discardSet.end())
			return;

		std::string_view& str = m_symbols[id];

		if (!str.empty())
			m_pool.AppendRetainedStringSize(str.size());

		m_discardSet.insert(id);
	}

	bool IsDiscarded(const SymbolId_t id) const noexcept
	{
		return m_discardSet.find(id) != m_discardSet.end();
	}

	inline size_t StringBytesTotal() const noexcept { return m_pool.StringBytesTotal(); }
	inline size_t StringBytesDiscarded() const noexcept { return m_pool.StringBytesDiscarded(); }
	inline size_t StringBytesRetained() const noexcept { return m_pool.StringBytesRetained(); }

	inline size_t NumStringsTotal() const noexcept { return m_pool.NumStringsTotal(); }
	inline size_t NumStringsDiscarded() const noexcept { return m_pool.NumStringsDiscarded(); }
	inline size_t NumStringsRetained() const noexcept { return m_pool.NumStringsRetained(); }

private:
	struct CaseInsensitiveHash_s
	{
		using is_transparent = void;
		size_t operator()(std::string_view s) const noexcept
		{
			uint64_t hash = 1469598103934665603ULL;
			for (unsigned char c : s)
			{
				hash ^= static_cast<uint64_t>(std::tolower(c));
				hash *= 1099511628211ULL;
			}
			return static_cast<size_t>(hash);
		}
	};

	struct CaseInsensitiveEqual_s
	{
		using is_transparent = void;
		bool operator()(std::string_view a, std::string_view b) const noexcept
		{
			if (a.size() != b.size())
				return false;

			for (size_t i = 0; i < a.size(); ++i)
			{
				if (std::tolower(static_cast<uint8_t>(a[i])) !=
					std::tolower(static_cast<uint8_t>(b[i])))
					return false;
			}

			return true;
		}
	};

	using LookupSensitive = std::unordered_map<std::string_view, SymbolId_t>;
	using LookupInsensitive = std::unordered_map<std::string_view, SymbolId_t,
		CaseInsensitiveHash_s, CaseInsensitiveEqual_s>;

	auto& GetLookup()
	{
		if constexpr (CaseInsensitive)
			return m_lookupInsensitive;
		else
			return m_lookupSensitive;
	}
	const auto& GetLookup() const
	{
		if constexpr (CaseInsensitive)
			return m_lookupInsensitive;
		else
			return m_lookupSensitive;
	}

	CStringPool m_pool;
	std::vector<std::string_view> m_symbols;
	std::unordered_set<SymbolId_t> m_discardSet;
	LookupSensitive m_lookupSensitive;
	LookupInsensitive m_lookupInsensitive;
};
