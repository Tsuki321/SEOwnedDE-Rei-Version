#include "NetVars.h"

#include <mutex>
#include <unordered_map>

namespace
{
	struct OffsetKey
	{
		RecvTable *m_pTable = nullptr;
		hash::hash_t m_nName = 0;

		bool operator==(const OffsetKey &other) const
		{
			return m_pTable == other.m_pTable && m_nName == other.m_nName;
		}
	};

	struct OffsetKeyHash
	{
		size_t operator()(const OffsetKey &key) const
		{
			const auto table = reinterpret_cast<std::uintptr_t>(key.m_pTable);
			return (table >> 4) ^ static_cast<size_t>(key.m_nName);
		}
	};

	struct NetVarCache
	{
		ClientClass *m_pClassList = nullptr;
		std::unordered_map<hash::hash_t, RecvTable *> m_mapClasses = {};
		std::unordered_map<OffsetKey, int, OffsetKeyHash> m_mapOffsets = {};
		std::mutex m_Mutex = {};
	};

	NetVarCache &GetCache()
	{
		static NetVarCache cache = {};
		return cache;
	}

	bool FindOffset(RecvTable *table, hash::hash_t name, int &offset)
	{
		if (!table || !table->m_pProps)
			return false;

		for (int i = 0; i < table->m_nProps; ++i)
		{
			const RecvProp &prop = table->m_pProps[i];
			if (prop.m_pVarName && HASH_RT(prop.m_pVarName) == name)
			{
				offset = prop.GetOffset();
				return true;
			}

			if (auto dataTable = prop.GetDataTable())
			{
				int nestedOffset = 0;
				if (FindOffset(dataTable, name, nestedOffset))
				{
					offset = nestedOffset + prop.GetOffset();
					return true;
				}
			}
		}

		return false;
	}

	int GetCachedOffset(NetVarCache &cache, RecvTable *table, hash::hash_t name)
	{
		const OffsetKey key = { table, name };
		if (const auto it = cache.m_mapOffsets.find(key); it != cache.m_mapOffsets.end())
			return it->second;

		int offset = 0;
		FindOffset(table, name, offset);
		cache.m_mapOffsets.emplace(key, offset);
		return offset;
	}

	void RefreshClasses(NetVarCache &cache, ClientClass *classList)
	{
		if (cache.m_pClassList == classList)
			return;

		cache.m_pClassList = classList;
		cache.m_mapClasses.clear();
		cache.m_mapOffsets.clear();

		for (auto node = classList; node; node = node->m_pNext)
		{
			if (node->m_pNetworkName && node->m_pRecvTable)
				cache.m_mapClasses.try_emplace(HASH_RT(node->m_pNetworkName), node->m_pRecvTable);
		}
	}
}

int NetVars::GetOffset(RecvTable *pTable, const char *szNetVar)
{
	if (!pTable || !szNetVar)
		return 0;

	auto &cache = GetCache();
	const std::scoped_lock lock(cache.m_Mutex);
	return GetCachedOffset(cache, pTable, HASH_RT(szNetVar));
}

int NetVars::GetNetVar(const char *szClass, const char *szNetVar)
{
	if (!I::BaseClientDLL || !szClass || !szNetVar)
		return 0;

	auto &cache = GetCache();
	const std::scoped_lock lock(cache.m_Mutex);

	auto classList = I::BaseClientDLL->GetAllClasses();
	RefreshClasses(cache, classList);

	const auto table = cache.m_mapClasses.find(HASH_RT(szClass));
	if (table == cache.m_mapClasses.end())
		return 0;

	return GetCachedOffset(cache, table->second, HASH_RT(szNetVar));
}
