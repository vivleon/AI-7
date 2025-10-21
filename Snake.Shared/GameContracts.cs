// Snake.Shared/GameContracts.cs
using System.Collections.Generic;
using System.Drawing;

namespace Snake.Shared
{
    public enum InputKey { Up, Down, Left, Right }
    public enum JoinResult { Ok, Full, Denied }
    public enum MatchPhase { Waiting, Playing, Finished }

    // ─── 멀티 조인/인풋 ───────────────────────────────────────────────────────────
    // ★ 비공개 방을 위해 Password 추가
    public record JoinRequest(string PlayerName, string RoomId, string? SelectedCosmeticId = null, string? Password = null);

    public record JoinAccepted(
        JoinResult Result,
        string PlayerId,
        string RoomId,
        int TickRate,
        CosmeticInstance Cosmetic,
        int AccountLevel,
        int AccountXp,
        string? SelectedCosmeticId = null
    );

    public record InputCommand(string PlayerId, int ClientSeq, InputKey Key, long ClientSentAtMs);

    // ─── 게임 스냅샷 ────────────────────────────────────────────────────────────
    public record PlayerStateDto(
        string PlayerId,
        string Name,
        int Level,
        List<Point> Body,
        bool Alive,
        int Score,
        CosmeticInstance Cosmetic
    );

    public enum LootKind { Gold, Xp }

    public record LootDropDto( // 드랍(위치+수치)
        int Id,
        Point Pos,
        int Amount,
        LootKind Kind
    );

    public record GameSnapshot(
        string RoomId,
        int ServerTick,
        Point Apple,
        List<PlayerStateDto> Players,
        MatchPhase Phase,
        int PhaseTicksRemaining)
    {
        public List<LootDropDto> Loot { get; init; } = new();
    }

    public record RankRewardDto(string PlayerId, string Name, int Rank, int GainedXp, int GainedCoins, bool NewCosmeticUnlocked, string? UnlockedCosmeticId);
    public record MatchFinishedDto(string RoomId, int ServerTick, List<RankRewardDto> Rewards, int YourLevel, int YourXp);
    public record RoomBrief(string RoomId, int Players, int Ready, MatchPhase Phase);

    // cosmetics/theme
    public record SnakeSkin(string BodyColorHex, string HeadCap, string TailCap, string Pattern);
    public record CosmeticInstance(string Id, string DisplayName, SnakeSkin Skin);

    // shop / inventory
    //public record ShopItemDto(string Id, string Display, int Price, int MinLevel, bool Owned);
    public sealed record ShopItemDto(
        string Id,
        string Display,
        int Price,
        int MinLevel,
        bool Owned
    );

    public record InventoryDto(
        List<string> OwnedCosmetics,
        List<string> OwnedThemes,
        int Coins,
        string? SelectedCosmeticId,
        string? SelectedThemeId,
        string? EmojiTag
    );

    public record AccountProfileDto(
        int Level, int Xp, int Coins,
        string? SelectedCosmeticId,
        string? SelectedThemeId,
        string? EmojiTag
    );

    public record SetSelectionRequest(string? CosmeticId, string? ThemeId);
    public record EmojiRequest(string Emoji);

    // ─── 계정/로그인 확장 ───────────────────────────────────────────────────────
    public record LoginResultDto(string Nick, AccountProfileDto Profile);

    // ─── 방 생성 API(허브) ──────────────────────────────────────────────────────
    public record CreateRoomRequest(string Title, bool IsPrivate, string? Password);
    public record CreateRoomResult(bool Ok, string RoomId, string Message);
}
