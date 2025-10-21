using System.Linq;

namespace Snake.Server.Services;

public class RewardService
{
    public (int xp, int coins) Calculate(int rank, int score)
    {
        var baseXp = Math.Max(10, score * 5);
        var baseCoins = Math.Max(5, score * 2);
        var rankBonus = rank switch { 1 => 40, 2 => 25, 3 => 15, _ => 0 };
        var coinBonus = rank switch { 1 => 30, 2 => 15, 3 => 10, _ => 0 };
        return (baseXp + rankBonus, baseCoins + coinBonus);
    }

    public string? TryUnlock(int effectiveLevel)
    {
        var candidates = Snake.Shared.CosmeticCatalog.UnlockByLevel
            .Where(x => x.MinLevel <= effectiveLevel)
            .Select(x => x.Id).ToList();

        if (candidates.Count == 0) return null;
        return (Random.Shared.NextDouble() < 0.10)
            ? candidates[Random.Shared.Next(candidates.Count)]
            : null;
    }
}


//public async Task<GachaResultDto> RollOneAsync(string playerName, string bannerId, CancellationToken ct = default)
//{
//    var now = DateTime.UtcNow;
//    var banners = await GetGachaAsync(now, ct);
//    var b = banners.First(x => x.BannerId == bannerId);
//    // pity count can be loaded from DB; here we infer from recent pulls (simplified for patch)
//    int pity = 0;
//    var pool = b.Pool;

//    // choose rarity by probs (apply pity)
//    var rnd = _rand.Next(100);
//    var rarityPick = Rarity.Common;
//    if (pity >= GachaBalance.PityEpicAt) rarityPick = Rarity.Epic;
//    else if (pity >= GachaBalance.PityRareAt) rarityPick = Rarity.Rare;
//    else
//    {
//        int acc = 0;
//        foreach (var kv in GachaBalance.Probabilities)
//        {
//            acc += kv.Value;
//            if (rnd < acc) { rarityPick = kv.Key; break; }
//        }
//    }
//    var candidates = pool.Where(x => x.Rarity == rarityPick).ToList();
//    if (candidates.Count == 0) candidates = pool.ToList();
//    var item = candidates[_rand.Next(candidates.Count)];

//    // grant or refund shards if duplicate
//    var own = await _shop.HasItemAsync(playerName, item.ItemId, ct);
//    if (own)
//    {
//        var shards = GachaBalance.ShardRefund[item.Rarity];
//        await _shop.AddShardsAsync(playerName, shards, ct);
//        return new GachaResultDto(item.ItemId, item.Display, item.Type, item.Rarity, IsNew:false, ShardRefund: shards);
//    }
//    else
//    {
//        await _shop.GrantAsync(playerName, item, ct);
//        return new GachaResultDto(item.ItemId, item.Display, item.Type, item.Rarity, IsNew:true, ShardRefund: 0);
//    }
//}
