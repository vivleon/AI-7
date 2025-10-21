//Snake.Shared/PurchaseResultDto.cs

namespace Snake.Shared;

public sealed class PurchaseResultDto
{
    public bool Ok { get; }
    public string Message { get; }
    public int Coins { get; }   // 필요 없는 곳은 0으로 내려도 OK

    // coins는 기본값 0으로
    public PurchaseResultDto(bool ok, string message, int coins = 0)
    {
        Ok = ok; Message = message; Coins = coins;
    }
}
