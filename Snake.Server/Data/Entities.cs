// Snake.Server/Data/Entities.cs
using System;
using System.ComponentModel.DataAnnotations;

namespace Snake.Server.Data;

public class Player
{
    [Key] public int Id { get; set; }
    [Required, MaxLength(40)] public string Name { get; set; } = "";
    public int Level { get; set; }
    public int Xp { get; set; }
    public int Coins { get; set; }
    public string? SelectedCosmeticId { get; set; }
    public string? SelectedThemeId { get; set; }
    public string? EmojiTag { get; set; }
}

public class PlayerCosmetic
{
    [Key] public int Id { get; set; }
    [Required] public string PlayerName { get; set; } = "";
    [Required] public string CosmeticId { get; set; } = "";
}

// ★ 추가: 보유 테마
public class PlayerTheme
{
    [Key] public int Id { get; set; }
    [Required] public string PlayerName { get; set; } = "";
    [Required] public string ThemeId { get; set; } = "";
}

public class Match
{
    [Key] public int Id { get; set; }
    [Required] public string RoomId { get; set; } = "";
    public DateTime StartedAt { get; set; }
    public DateTime EndedAt { get; set; }
}

public class MatchPlayer
{
    [Key] public int Id { get; set; }
    [Required] public int MatchId { get; set; }
    [Required] public string PlayerName { get; set; } = "";
    public int Rank { get; set; }
    public int Score { get; set; }
    public int Length { get; set; }
}

public class MovementLog
{
    [Key] public int Id { get; set; }
    public int MatchId { get; set; }
    public string PlayerName { get; set; } = "";
    public int Tick { get; set; }
    public int X { get; set; }
    public int Y { get; set; }
    public bool Alive { get; set; }
    public int Score { get; set; }
    public DateTime Utc { get; set; }
}

// ★ 신규: 계정
public class Account
{
    [Key] public int Id { get; set; }
    [Required, MaxLength(40)] public string LoginId { get; set; } = "";
    [Required, MaxLength(128)] public string PasswordHash { get; set; } = "";
    [Required, MaxLength(40)] public string Nick { get; set; } = "";
    [MaxLength(40)] public string Name { get; set; } = "";
    [MaxLength(10)] public string Gender { get; set; } = "";
}
