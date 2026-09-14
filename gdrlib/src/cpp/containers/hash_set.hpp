#pragma once

#include <assert2.hpp>
#include <cpp/alg_constexpr.hpp>
#include <cpp/hash/crc_hash.hpp>
#include <pod_types.hpp>

namespace cpp
{
    template<typename T>
    struct hash_lookup
    {
    private:
        u64 m_buckets {0};
        T* m_table {nullptr};
        u32* m_occupancy {nullptr};
        u32* m_tombstones {nullptr};

        constexpr static u32 kMod = sizeof(u32) * 8;

    public:
        hash_lookup() = default;

        hash_lookup(const hash_lookup& other)            = delete;
        hash_lookup& operator=(const hash_lookup& other) = delete;

        ~hash_lookup() { cleanup(); }

        hash_lookup(u64 size) { refit(size + 1); }

        void set(const T& key, const u64 hash)
        {
            u64 bucket = get(key, hash);
            if (!occupied(bucket))
            {
                occupy(bucket);
                unentomb(bucket);
                m_table[bucket] = key;
            }
        }

        void erase(const T& key, u64 hash)
        {
            const u64 bucket = get(key, hash);
            if (!occupied(bucket))
            {
                return;
            }

            unoccupy(bucket);
            entomb(bucket);
        }

        bool has(const T& key, u64 hash) { return occupied(get(key, hash)); }

        void set(const T& key) { set(key, cpp::crc::crc64(reinterpret_cast<const char*>(&key), sizeof(T))); }

        void erase(const T& key) { erase(key, cpp::crc::crc64(reinterpret_cast<const char*>(&key), sizeof(T))); }

        bool empty() const
        {
            for (u32 i = 0; i < cells(); ++i)
            {
                if (m_occupancy[i] > 0)
                {
                    return false;
                }
            }

            return true;
        }

    private:
        [[nodiscard]] bool occupied(u32 index) const { return (m_occupancy[index / kMod] >> (index % kMod)) & 1; }

        [[nodiscard]] bool tombstoned(u32 index) const { return (m_tombstones[index / kMod] >> (index % kMod)) & 1; }

        void occupy(u32 index) { m_occupancy[index / kMod] |= 1U << (index % kMod); }

        void unoccupy(u32 index) { m_occupancy[index / kMod] &= ~(1U << (index % kMod)); }

        void entomb(u32 index) { m_tombstones[index / kMod] |= 1U << (index % kMod); }

        void unentomb(u32 index) { m_tombstones[index / kMod] &= ~(1U << (index % kMod)); }

        u64 cells() const { return (m_buckets / kMod) + 1; }

        void cleanup()
        {
            delete[] m_table;
            delete[] m_occupancy;
            delete[] m_tombstones;
        }

        void refit(const u64 size)
        {
            m_buckets = 1;

            while (m_buckets < size)
            {
                m_buckets <<= 1;
            }

            const u64 occupancy_cells = cells();

            cleanup();

            m_table      = new T[m_buckets];
            m_occupancy  = new u32[occupancy_cells];
            m_tombstones = new u32[occupancy_cells];

            cpp::cx_fill(m_occupancy + 0, m_occupancy + occupancy_cells, 0);
            cpp::cx_fill(m_tombstones + 0, m_tombstones + occupancy_cells, 0);
        }

        // https://github.com/zeux/meshoptimizer/blob/c93ba0987baa84bd73b61edf1c0ba7ba2e48df4b/src/simplifier.cpp
        u64 get(const T& key, const u64 hash)
        {
            assert2m(m_buckets > 0, "hash lookup was not initialized");
            assert2m((m_buckets & (m_buckets - 1)) == 0, "m_buckets must be a power of 2");

            u64 hashmod   = m_buckets - 1;
            u64 bucket    = hash & hashmod;
            u64 tombstone = hashmod + 1;  // unreachable value

            for (u64 probe = 0; probe <= hashmod; ++probe)
            {
                if (occupied(bucket) && m_table[bucket] == key)
                {
                    return bucket;
                }

                if (!occupied(bucket) && tombstoned(bucket))
                {
                    tombstone = tombstone > hashmod ? bucket : tombstone;
                }
                else if (!occupied(bucket))
                {
                    return tombstone > hashmod ? bucket : tombstone;
                }

                // hash collision, quadratic probing
                bucket = (bucket + probe + 1) & hashmod;
            }

            assert2m(false, "Hash table is full");
            return 0;
        }
    };

    using hash_set = hash_lookup<u64>;
}
