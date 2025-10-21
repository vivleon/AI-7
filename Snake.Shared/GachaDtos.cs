// Snake.Shared/GachaDtos.cs
using System;
using System.Collections.Generic;

namespace Snake.Shared
{
    public enum Rarity { Common, Rare, Epic, Legendary }

    // 가챠에서 노출/획득되는 아이템 정보(코스메틱/이펙트/기타)
    public sealed record GachaItemDto(string Id, string Display, string Kind, Rarity Rarity);

    public sealed record GachaBannerDto(
        string BannerId,
        string Title,
        DateTime StartUtc,
        DateTime EndUtc,
        List<GachaItemDto> Pool
    );

    public sealed record GachaPullRequest(string PlayerName, string BannerId, int Count);

    // ✅ 한 개 결과 아이템 DTO (tuple 대신 명시 타입)
    public sealed record GachaResultItemDto(
        string Id,
        bool IsNew,
        Rarity Rarity,
        int ShardGain
    );

    // ✅ Results 타입을 DTO 리스트로 고정
    public sealed record GachaPullResult(
        int SpentCoins,
        int ShardsDelta,
        List<GachaResultItemDto> Results,
        int TotalShards,
        int RemainingCoins
    );

    public sealed record CraftRequest(string PlayerName, string ItemId);

    public sealed record CraftResult(
        bool Ok,
        string Message,
        string CraftedId,
        int CostShards,
        int TotalShards
    );

    public sealed record ShardInfoDto(int Shards);
}

namespace Snake.Shared
{
    public static class GachaBalance
    {
        // Rarity probability(%) – total 100 (참고용)
        public static readonly Dictionary<Rarity, int> Probabilities = new()
        {
            { Rarity.Common,    62 },
            { Rarity.Rare,      25 },
            { Rarity.Epic,      10 },
            { Rarity.Legendary,  3  },
        };

        // Duplicate -> shard refund
        public static readonly Dictionary<Rarity, int> ShardRefund = new()
        {
            { Rarity.Common,    10 },
            { Rarity.Rare,      25 },
            { Rarity.Epic,      75 },
            { Rarity.Legendary, 250 },
        };

        // Pity: (지금은 미사용) 10연 ≥ Rare, 30연 ≥ Epic
        public const int PityRareAt = 10;
        public const int PityEpicAt = 30;
    }
}
