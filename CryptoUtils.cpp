#include "CryptoUtils.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <cstring>

#ifdef HAVE_ARGON2
#include <argon2.h>
#endif

// ── 내부 유틸 ────────────────────────────────────────────────────────────────
static QByteArray INT32BE(uint32_t i) {
    QByteArray b(4, 0);
    b[0] = char((i >> 24) & 0xFF);
    b[1] = char((i >> 16) & 0xFF);
    b[2] = char((i >> 8)  & 0xFF);
    b[3] = char((i)       & 0xFF);
    return b;
}

// constant-time 비교 (길이 차이 포함해 끝까지 순회)
static bool constantTimeEqual(const QByteArray& a, const QByteArray& b) {
    const int na = a.size();
    const int nb = b.size();
    const int n  = (na > nb ? na : nb);
    unsigned char diff = static_cast<unsigned char>(na ^ nb);
    for (int i = 0; i < n; ++i) {
        const unsigned char aa = i < na ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char bb = i < nb ? static_cast<unsigned char>(b[i]) : 0;
        diff |= (aa ^ bb);
    }
    return diff == 0;
}

// ── HMAC-SHA256 ─────────────────────────────────────────────────────────────
static QByteArray hmacSha256(const QByteArray& keyIn, const QByteArray& data) {
    const int block = 64;
    QByteArray key = keyIn;
    if (key.size() > block)
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    if (key.size() < block)
        key.append(QByteArray(block - key.size(), char(0x00)));

    QByteArray o(block, char(0x5c)), i(block, char(0x36));
    for (int n=0;n<block;++n) {
        o[n] = o[n] ^ key[n];
        i[n] = i[n] ^ key[n];
    }
    QByteArray inner = QCryptographicHash::hash(i + data, QCryptographicHash::Sha256);
    return QCryptographicHash::hash(o + inner, QCryptographicHash::Sha256);
}

// ── PBKDF2(HMAC-SHA256) ─────────────────────────────────────────────────────
QByteArray CryptoUtils::pbkdf2(const QByteArray &password, const QByteArray &salt,
                               int iterations, int dkLen)
{
    const int hLen = 32; // SHA256
    const int l = (dkLen + hLen - 1) / hLen;
    QByteArray dk; dk.reserve(l * hLen);

    for (int i = 1; i <= l; ++i) {
        QByteArray u = hmacSha256(password, salt + INT32BE(uint32_t(i)));
        QByteArray t = u;
        for (int j = 1; j < iterations; ++j) {
            u = hmacSha256(password, u);
            for (int k = 0; k < hLen; ++k)
                t[k] = char(uchar(t[k]) ^ uchar(u[k]));
        }
        dk.append(t);
    }
    dk.truncate(dkLen);
    return dk;
}

// ── 확장 API ────────────────────────────────────────────────────────────────
QByteArray CryptoUtils::randomBytes(int n) {
    QByteArray out; out.resize(n);
    for (int i=0; i<n; i+=4) {
        quint32 r = QRandomGenerator::system()->generate();
        const int remain = qMin(4, n - i);
        std::memcpy(out.data() + i, &r, size_t(remain));
    }
    return out;
}

bool CryptoUtils::haveArgon2() {
#ifdef HAVE_ARGON2
    return true;
#else
    return false;
#endif
}

StoredPw CryptoUtils::hashPassword(const QString& raw, bool preferArgon2) {
#ifdef HAVE_ARGON2
    if (preferArgon2) {
        // Argon2id 파라미터(권장값 예시)
        const uint32_t t_cost   = 3;          // iterations
        const uint32_t m_cost   = 1u << 16;   // 64MB
        const uint32_t parallel = 1;

        QByteArray pwd  = raw.toUtf8();
        QByteArray salt = randomBytes(16);
        QByteArray encoded(256, 0);

        int rc = argon2id_hash_encoded(t_cost, m_cost, parallel,
                                       pwd.constData(), size_t(pwd.size()),
                                       salt.constData(), size_t(salt.size()),
                                       32, encoded.data(), encoded.size());
        if (rc == ARGON2_OK) {
            StoredPw out;
            out.algo = PwAlgo::ARGON2ID;
            out.hash = QByteArray(encoded.constData()); // C-string → 길이 자동
            out.iterations = 0;
            return out;
        }
        // 실패시 PBKDF2로 폴백
    }
#endif
    StoredPw out;
    out.algo = PwAlgo::PBKDF2;
    out.salt = randomBytes(16);
    out.iterations = 120000;
    out.hash = pbkdf2(raw.toUtf8(), out.salt, out.iterations, 32);
    return out;
}

bool CryptoUtils::verifyPassword(const QString& raw, const StoredPw& stored) {
    if (stored.algo == PwAlgo::PBKDF2) {
        if (stored.salt.isEmpty() || stored.hash.isEmpty() || stored.iterations <= 0) return false;
        QByteArray dk = pbkdf2(raw.toUtf8(), stored.salt, stored.iterations, stored.hash.size());
        return constantTimeEqual(dk, stored.hash);
    }
#ifdef HAVE_ARGON2
    if (stored.algo == PwAlgo::ARGON2ID) {
        if (stored.hash.isEmpty()) return false;
        QByteArray pwd = raw.toUtf8();
        int rc = argon2id_verify(stored.hash.constData(), pwd.constData(), size_t(pwd.size()));
        return rc == ARGON2_OK;
    }
#endif
    return false;
}
