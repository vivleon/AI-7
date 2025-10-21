// LanguageManager.cpp
#include "LanguageManager.h"


static QString normalizeKey(const QString& k) {
    // 키는 그대로 쓰되, 앞뒤 공백 제거
    return k.trimmed();
}

LanguageManager& LanguageManager::inst() {
    static LanguageManager g;
    return g;
}

LanguageManager::LanguageManager() : QObject(nullptr)
{
    ko_.clear(); ko_.reserve(256);

    // Menus
    ko_["File"] = "파일"; ko_["View"] = "보기"; ko_["AI"] = "인공지능"; ko_["Admin"] = "관리"; ko_["Language"] = "언어";
    ko_["System Default"] = "시스템 기본"; ko_["Korean"] = "한국어"; ko_["English"] = "영어";

    // Common labels
    ko_["Time"]="시간"; ko_["Value"]="값"; ko_["Avg"]="평균"; ko_["Min"]="최소"; ko_["Max"]="최대"; ko_["Levels"]="레벨"; ko_["Count"]="건수"; ko_["points"]="포인트"; ko_["ON"]="켜짐"; ko_["OFF"]="꺼짐"; ko_["Avg Value"]="평균값";

    // File/View items
    ko_["🔒 Lock"]="🔒 잠금"; ko_["⬇️ Export (CSV/TXT)"]="⬇️ 내보내기 (CSV/TXT)"; ko_["🚪 Logout"]="🚪 로그아웃"; ko_["⏻ Exit"]="⏻ 종료";
    ko_["🪟 Split"]="🪟 분할 보기"; ko_["🌡️ Focus: Temp"]="🌡️ 온도 차트만 보기"; ko_["📳 Focus: Vib"]="📳 진동 차트만 보기"; ko_["🛡️ Focus: Intr"]="🛡️ 침입 차트만 보기"; ko_["📜 Focus: Logs"]="📜 로그만 보기";

    // AI menu
    ko_["KPI (Charts)"]="KPI (차트)"; ko_["KPI Summary (Text)"]="KPI 요약 (텍스트)"; ko_["Trend + Forecast"]="추세 + 예측"; ko_["XAI Notes"]="설명형 AI 노트"; ko_["Report Editor (PDF)"]="리포트 편집기 (PDF)"; ko_["Rule Simulator"]="규칙 시뮬레이터"; ko_["RCA (Co-occurrence)"]="RCA (동시발생)"; ko_["NL Query…"]="자연어 질의…"; ko_["Sensor Health"]="센서 상태 진단"; ko_["Tuning Impact (14d)"]="튜닝 영향 (14일)";

    // Admin menu
    ko_["👤 User Management"]="👤 사용자 관리"; ko_["Alert Thresholds"]="임계치 설정"; ko_["🔍 Audit Logs"]="🔍 감사 로그"; ko_["👥 Active Sessions"]="👥 현재 접속자"; ko_["Active Sessions"]="현재 접속자";

    // Chart/type/levels
    ko_["Temperature"]="온도"; ko_["Vibration"]="진동"; ko_["Intrusion"]="침입"; ko_["Warning"]="주의"; ko_["High"]="경고"; ko_["Warn"]="주의";
    ko_["TEMP"]="온도"; ko_["VIB"]="진동"; ko_["INTR"]="침입";

    // Status
    ko_["Dashboard initialized"]="대시보드 초기화 완료"; ko_["Filter applied"]="필터 적용됨"; ko_["Filter cleared (1y~now)"]="필터 초기화 (1년~현재)"; ko_["CSV exported"]="CSV로 내보냈습니다"; ko_["TXT exported"]="TXT로 내보냈습니다"; ko_["Logged in"]="로그인되었습니다";

    // Roles (status bar)
    ko_["Role: Viewer"]="역할: Viewer"; ko_["Role: Operator"]="역할: Operator"; ko_["Role: Analyst"]="역할: Analyst"; ko_["Role: Admin"]="역할: Admin";

    // Dialog/common
    ko_["Save"]="저장"; ko_["Cancel"]="취소"; ko_["Confirm"]="확인"; ko_["Export"]="내보내기"; ko_["Cannot open file."]="파일을 열 수 없습니다"; ko_["Cannot open file: %1"]="파일을 열 수 없습니다: %1";

    // Audit dialog
    ko_["🧾 Audit Logs"]="🧾 감사 로그"; ko_["From:"]="시작:"; ko_["To:"]="종료:"; ko_["actor (username)"]="사용자"; ko_["action (contains)"]="행동 (포함)"; ko_["Search"]="검색"; ko_["Export CSV"]="CSV 내보내기";

    // NLQ helper
    ko_["Type a question about your data (one line)."]="데이터에 대한 질문을 한 줄로 입력하세요."; ko_["Examples:"]="예시:"; ko_["List HIGH VIB events last week"]="지난주 HIGH VIB 발생 목록"; ko_["Average TEMP by sensor for yesterday"]="어제 센서별 TEMP 평균"; ko_["Top 10 sensors with HIGH INTR count in last 24h"]="최근 24시간 HIGH INTR 건수 상위 10개 센서"; ko_["Show TEMP trend and forecast for next 30 min"]="향후 30분 TEMP 추세/예측 표시"; ko_["e.g., last week VIB HIGH events"]="예) 지난주 VIB HIGH 발생"; ko_["Run"]="실행";

    // Auth
    ko_["Login"]="로그인"; ko_["Username"]="아이디"; ko_["Password"]="비밀번호";

    // Log table
    ko_["SensorId"]="센서ID"; ko_["Type"]="타입"; ko_["Level"]="위험도"; ko_["DateTime"]="일시"; ko_["Lat"]="위도"; ko_["Lng"]="경도"; ko_["All"]="전체";

    // Level values
    ko_["LOW"]="낮음"; ko_["MEDIUM"]="주의"; ko_["HIGH"]="경고";

    // Export
    ko_["Export TXT"]="TXT 내보내기";

    // Misc
    ko_["Thresholds updated"]="임계치가 업데이트되었습니다";

    // AI toolbar/report
    ko_["Export PDF"]="PDF 내보내기"; ko_["Print"]="인쇄"; ko_["Period:"]="기간:"; ko_["KPI Summary"]="KPI 요약"; ko_["Trend"]="추세"; ko_["XAI Highlights"]="XAI 하이라이트"; ko_["Top 10 HIGH distribution"]="상위 10건 HIGH 분포"; ko_["Checks recommended: TEMP↑(cooling/vent), VIB↑(bearing/alignment), INTR↑(access control)"] = "권장 점검: TEMP↑(냉각/환기), VIB↑(베어링/정렬), INTR↑(출입통제)"; ko_["No significant HIGH in range."] = "분석 구간에서 HIGH 이상이 거의 없습니다."; ko_["Tuning History"] = "튜닝 히스토리"; ko_["No related audit logs"] = "관련 감사 로그 없음"; ko_["HIGH mitigation (hypothetical)"] = "HIGH 억제 효과(가정)"; ko_["Slope"] = "기울기";

    // Session dialog labels
    ko_["User"]="사용자"; ko_["Client"]="클라이언트"; ko_["Created"]="생성"; ko_["Last Seen"]="최근"; ko_["ID"]="ID"; ko_["Refresh"]="새로고침";

    // Role names (for completeness)
    ko_["Viewer"]="Viewer"; ko_["Operator"]="Operator"; ko_["Analyst"]="Analyst"; ko_["Admin"]="Admin"; ko_["SuperAdmin"]="SuperAdmin"; ko_["Active"]="활성"; ko_["Inactive"]="비활성";

    ko_["Role"] = "역할";
}


void LanguageManager::set(LanguageManager::Lang l) {
    if (cur_ == l) return;
    cur_ = l;
    emit languageChanged(l);
}
LanguageManager::Lang LanguageManager::current() const { return cur_; }


QString LanguageManager::t(const QString& key) const {
    const auto it = dict_.constFind(key);
    return (it == dict_.cend()) ? key : *it;   // ✅ 참조/뷰 보관 안 함
}
