#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include <QByteArray>
#include <QString>

enum class PwAlgo { PBKDF2, ARGON2ID };

struct StoredPw {
    PwAlgo     algo = PwAlgo::PBKDF2;
    QByteArray hash;       // PBKDF2: raw bytes, Argon2: encoded string
    QByteArray salt;       // PBKDF2 only
    int        iterations = 0; // PBKDF2 only
};

class CryptoUtils {
public:
    // 기존 호환 API (PBKDF2-HMAC-SHA256)
    static QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt,
                             int iterations, int dkLen);

    // 랜덤 바이트
    static QByteArray randomBytes(int n);

    // Argon2 사용 가능 여부(컴파일 시점)
    static bool haveArgon2();

    // 새 해시 생성 (preferArgon2=true면 Argon2id 우선, 실패 시 PBKDF2)
    static StoredPw hashPassword(const QString& raw, bool preferArgon2);

    // 저장된 해시 검증
    static bool verifyPassword(const QString& raw, const StoredPw& stored);
};

#endif // CRYPTOUTILS_H
