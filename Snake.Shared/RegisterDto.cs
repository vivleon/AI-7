//Snake.Shared/RegisterDto.cs

namespace Snake.Shared;

public record RegisterDto(
    string LoginId,
    string Password,
    string PasswordConfirm,
    string Name,
    string Gender,
    string Nick
);
