// Snake.Shared / OnlinePlayerBrief.cs
namespace Snake.Shared;
public record OnlinePlayerBrief(string Name, int Level, bool InRoom, string? RoomId, bool IsHost);
