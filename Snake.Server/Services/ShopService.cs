// Snake.Server/Services/ShopService.cs  (전체 교체본 중 하단에 메서드 추가)
using Microsoft.EntityFrameworkCore;
using Snake.Server.Data;
using Snake.Shared;

namespace Snake.Server.Services;

public class ShopService
{
    private readonly IDbContextFactory<AppDbContext> _db;

    public ShopService(IDbContextFactory<AppDbContext> db) => _db = db;

    // ★ 플레이어가 없으면 자동 생성
    private static async Task<Data.Player> EnsurePlayerAsync(AppDbContext db, string playerName)
    {
        var p = await db.Players.FirstOrDefaultAsync(x => x.Name == playerName);
        if (p != null) return p;

        p = new Data.Player
        {
            Name = playerName,
            Level = 1,
            Xp = 0,
            Coins = 0,
            SelectedCosmeticId = "skin_basic_blue",
            SelectedThemeId = "theme_dark",
            EmojiTag = null
        };
        db.Players.Add(p);
        await db.SaveChangesAsync();
        return p;
    }

    public async Task<AccountProfileDto> GetProfileAsync(string playerName)
    {
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);
        return new AccountProfileDto(
            p.Level, p.Xp, p.Coins,
            p.SelectedCosmeticId, p.SelectedThemeId, p.EmojiTag
        );
    }

    public async Task<InventoryDto> GetInventoryAsync(string playerName)
    {
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);

        var ownCos = await db.PlayerCosmetics
            .Where(x => x.PlayerName == playerName).Select(x => x.CosmeticId).ToListAsync();

        var ownTheme = await db.PlayerThemes
            .Where(x => x.PlayerName == playerName).Select(x => x.ThemeId).ToListAsync();

        return new InventoryDto(
            ownCos, ownTheme, p.Coins,
            p.SelectedCosmeticId, p.SelectedThemeId, p.EmojiTag
        );
    }

    public async Task<List<ShopItemDto>> GetCosmeticCatalogAsync(string playerName)
    {
        using var db = await _db.CreateDbContextAsync();
        await EnsurePlayerAsync(db, playerName);

        var own = await db.PlayerCosmetics
            .Where(x => x.PlayerName == playerName).Select(x => x.CosmeticId).ToListAsync();

        return CosmeticCatalog.AllSkins
            .Select(s => new ShopItemDto(s.Id, s.Display, s.Price, s.MinLevel, own.Contains(s.Id)))
            .ToList();
    }

    public async Task<List<ShopItemDto>> GetThemeCatalogAsync(string playerName)
    {
        using var db = await _db.CreateDbContextAsync();
        await EnsurePlayerAsync(db, playerName);

        var own = await db.PlayerThemes
            .Where(x => x.PlayerName == playerName).Select(x => x.ThemeId).ToListAsync();

        return ThemeCatalog.AllThemes
            .Select(t => new ShopItemDto(t.Id, t.Display, t.Price, t.MinLevel, own.Contains(t.Id)))
            .ToList();
    }

    public async Task<PurchaseResultDto> BuyCosmeticAsync(string playerName, string id)
    {
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);

        var item = CosmeticCatalog.AllSkins.FirstOrDefault(x => x.Id == id);
        if (item is null) return new(false, "존재하지 않는 스킨", p.Coins);

        if (p.Level < item.MinLevel) return new(false, $"요구 레벨 {item.MinLevel} 이상 필요", p.Coins);
        var owned = await db.PlayerCosmetics.AnyAsync(x => x.PlayerName == playerName && x.CosmeticId == id);
        if (owned) return new(false, "이미 보유함", p.Coins);
        if (p.Coins < item.Price) return new(false, "코인 부족", p.Coins);

        p.Coins -= item.Price;
        db.PlayerCosmetics.Add(new PlayerCosmetic { PlayerName = playerName, CosmeticId = id });
        await db.SaveChangesAsync();
        return new(true, "구매 완료", p.Coins);
    }

    public async Task<PurchaseResultDto> BuyThemeAsync(string playerName, string id)
    {
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);

        var item = ThemeCatalog.AllThemes.FirstOrDefault(x => x.Id == id);
        if (item is null) return new(false, "존재하지 않는 테마", p.Coins);

        if (p.Level < item.MinLevel) return new(false, $"요구 레벨 {item.MinLevel} 이상 필요", p.Coins);
        var owned = await db.PlayerThemes.AnyAsync(x => x.PlayerName == playerName && x.ThemeId == id);
        if (owned) return new(false, "이미 보유함", p.Coins);
        if (p.Coins < item.Price) return new(false, "코인 부족", p.Coins);

        p.Coins -= item.Price;
        db.PlayerThemes.Add(new PlayerTheme { PlayerName = playerName, ThemeId = id });
        await db.SaveChangesAsync();
        return new(true, "구매 완료", p.Coins);
    }

    public async Task<bool> SetSelectionAsync(string playerName, SetSelectionRequest req)
    {
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);

        if (!string.IsNullOrWhiteSpace(req.CosmeticId))
        {
            var own = await db.PlayerCosmetics.AnyAsync(x => x.PlayerName == playerName && x.CosmeticId == req.CosmeticId);
            if (!own) return false;
            p.SelectedCosmeticId = req.CosmeticId;
        }
        if (!string.IsNullOrWhiteSpace(req.ThemeId))
        {
            var own = await db.PlayerThemes.AnyAsync(x => x.PlayerName == playerName && x.ThemeId == req.ThemeId);
            if (!own) return false;
            p.SelectedThemeId = req.ThemeId;
        }
        await db.SaveChangesAsync();
        return true;
    }

    public async Task<PurchaseResultDto> SetEmojiAsync(string playerName, string emoji)
    {
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);

        if (string.IsNullOrWhiteSpace(emoji)) emoji = null!;
        if (emoji != null && emoji.Length > 8)
            return new(false, "이모지는 최대 8자", p.Coins);
        if (emoji != null && emoji.Any(char.IsLetterOrDigit))
            return new(false, "문자/숫자는 불가(이모지만)", p.Coins);

        p.EmojiTag = string.IsNullOrWhiteSpace(emoji) ? null : emoji;
        await db.SaveChangesAsync();
        return new(true, "적용됨", p.Coins);
    }

    // ★ 추가: 멀티에서 보너스 코인 반영
    public async Task<bool> AddCoinsAsync(string playerName, int amount)
    {
        if (amount <= 0) return false;
        using var db = await _db.CreateDbContextAsync();
        var p = await EnsurePlayerAsync(db, playerName);
        p.Coins += amount;
        await db.SaveChangesAsync();
        return true;
    }
}
