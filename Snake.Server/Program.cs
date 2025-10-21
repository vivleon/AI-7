using System.Net;
using Microsoft.AspNetCore.HttpOverrides;
using Microsoft.AspNetCore.Http.Connections; // ★ MapHub 옵션 타입
using Microsoft.EntityFrameworkCore;
using Snake.Server;
using Snake.Server.Data;
using Snake.Server.Rooms;
using Snake.Server.Services;

var builder = WebApplication.CreateBuilder(args);

// ───────────────────────────────────────────────────────────────
// Kestrel/URLs
// - 개발(run) 땐 launchSettings.json의 0.0.0.0:5000/5001가 적용됨
// - 서비스로 띄우거나 외부에서 확실히 바인딩하려면 아래 두 가지 중 하나 사용:
//   1) 환경변수 ASPNETCORE_URLS="http://0.0.0.0:5000;https://0.0.0.0:5001"
//   2) appsettings.json → Kestrel:Endpoints 혹은 아래 UseUrls 주석 해제
//builder.WebHost.UseUrls("http://0.0.0.0:5000", "https://0.0.0.0:5001");

// ───────────────────────────────────────────────────────────────
// 서비스 등록
builder.Services.AddSignalR()
    .AddJsonProtocol(o => o.PayloadSerializerOptions.WriteIndented = false);

builder.Services.AddDbContextFactory<AppDbContext>(opt =>
    opt.UseSqlite(builder.Configuration.GetConnectionString("Default")));

builder.Services.AddScoped<ShopService>();

builder.Services.AddSingleton<RoomManager>();
builder.Services.AddSingleton<RewardService>();
builder.Services.AddSingleton<SnapshotMapper>();
builder.Services.AddHostedService<GameLoopHostedService>();

// 외부/프록시(리버스 프록시, 로드밸런서) 앞단에 둘 수 있으므로 설정
builder.Services.Configure<ForwardedHeadersOptions>(o =>
{
    o.ForwardedHeaders = ForwardedHeaders.XForwardedFor | ForwardedHeaders.XForwardedProto;
    // 필요시 신뢰 프록시 추가:
    // o.KnownProxies.Add(IPAddress.Parse("10.10.21.254"));
});

// (브라우저 클라이언트가 생길 가능성 대비) CORS – WinForms .NET 클라는 불필요하지만 무해
builder.Services.AddCors(o => o.AddDefaultPolicy(p =>
    p.AllowAnyHeader().AllowAnyMethod().AllowAnyOrigin()));

var app = builder.Build();

// ───────────────────────────────────────────────────────────────
// DB 마이그레이션/시드
await using (var scope = app.Services.CreateAsyncScope())
{
    var factory = scope.ServiceProvider.GetRequiredService<IDbContextFactory<AppDbContext>>();
    await using var db = await factory.CreateDbContextAsync();
    await DbBootstrap.EnsureUpToDateAsync(db);
    await Seed.RunAsync(db);
}

// ───────────────────────────────────────────────────────────────
// 미들웨어 파이프라인
app.UseForwardedHeaders();
app.UseCors();

// HTTPS 리다이렉트는 인증서가 정식으로 있을 때만 켜세요.
// (개발 환경/공인IP 직접접속 테스트일 땐 주석 유지)
// app.UseHttpsRedirection();

// 헬스체크/간단 핑 (NAT/방화벽/프록시 테스트용)
app.MapGet("/", () => Results.Text("Snake Server up"));
app.MapGet("/health", () => Results.Ok("OK"));

// ★ 허브 매핑: 엔드포인트에 AddFilter 붙이지 마세요.
// 필요 시 Dispatcher 옵션은 아래처럼 지정(타입 중요!)
app.MapHub<GameHub>("/hub", (HttpConnectionDispatcherOptions o) =>
{
    // 예시: 롱폴링 타임아웃, 버퍼 제한
    o.LongPolling.PollTimeout = TimeSpan.FromSeconds(90);
    o.TransportMaxBufferSize = 64 * 1024;
    o.ApplicationMaxBufferSize = 64 * 1024;
});

app.Run();
