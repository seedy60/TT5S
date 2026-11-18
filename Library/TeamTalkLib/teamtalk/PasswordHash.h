/*
 * Copyright (c) 2005-2018, BearWare.dk
 * 
 * Contact Information:
 *
 * Bjoern D. Rasmussen
 * Kirketoften 5
 * DK-8260 Viby J
 * Denmark
 * Email: contact@bearware.dk
 * Phone: +45 20 20 54 59
 * Web: http://www.bearware.dk
 *
 * This source code is part of the TeamTalk SDK owned by
 * BearWare.dk. Use of this file, or its compiled unit, requires a
 * TeamTalk SDK License Key issued by BearWare.dk.
 *
 * The TeamTalk SDK License Agreement along with its Terms and
 * Conditions are outlined in the file License.txt included with the
 * TeamTalk SDK distribution.
 *
 */

#if !defined(PASSWORDHASH_H)
#define PASSWORDHASH_H

#include <cstdint>
#include <string>

/* Password hashing for server user accounts.
 *
 * Passwords are never stored or logged in plain text. Stored
 * passwords use the Argon2id Password Hashing Scheme (PHC) string
 * format, e.g.:
 *
 *   $argon2id$v=19$m=19456,t=2,p=1$<salt>$<hash>
 *
 * The implementation is self-contained (no external Argon2 library is
 * required) and follows RFC 9106.
 */
namespace teamtalk
{
namespace password
{
    // RFC 9106 second recommended option, i.e. 19 MiB, 2 iterations,
    // 1 lane, 128-bit salt and 256-bit tag.
    constexpr uint32_t ARGON2ID_MEMORY_KB = 19456;
    constexpr uint32_t ARGON2ID_ITERATIONS = 2;
    constexpr uint32_t ARGON2ID_LANES = 1;
    constexpr uint32_t ARGON2ID_SALT_LENGTH = 16;
    constexpr uint32_t ARGON2ID_HASH_LENGTH = 32;

    // Returns true if 'stored' is an Argon2id PHC string.
    bool IsHashedPassword(const std::string& stored);

    // Returns an Argon2id PHC string for 'passwordUtf8' using a random
    // salt. An empty password returns an empty string since there is
    // nothing secret to protect (e.g. BearWare.dk WebLogin accounts).
    std::string HashPassword(const std::string& passwordUtf8);

    // Verifies 'passwordUtf8' against 'stored'. Legacy plain text
    // passwords (from older configuration files) are also accepted so
    // they can be upgraded to Argon2id hashes.
    bool VerifyPassword(const std::string& passwordUtf8, const std::string& stored);

} // namespace password
} // namespace teamtalk

#endif // !defined(PASSWORDHASH_H)
