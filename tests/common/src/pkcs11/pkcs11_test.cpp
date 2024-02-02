/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>
#include <gmock/gmock.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/pkcs11/pkcs11.hpp"
#include "aos/common/pkcs11/privatekey.hpp"
#include "aos/common/tools/allocator.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/uuid.hpp"

#include "../log.hpp"

using namespace testing;

namespace aos {
namespace pkcs11 {

/***********************************************************************************************************************
 * Suite
 **********************************************************************************************************************/

class PKCS11Test : public Test {
protected:
    void SetUp() override
    {
        setenv("SOFTHSM2_CONF", WORK_DIR "/softhsm/softhsm2.conf", true);

        Log::SetCallback([](LogModule module, LogLevel level, const String& message) {
            static std::mutex           sLogMutex;
            std::lock_guard<std::mutex> lock(sLogMutex);

            std::cout << level.ToString().CStr() << " | " << module.ToString().CStr() << " | " << message.CStr()
                      << std::endl;
        });

        mLibrary = mManager.OpenLibrary(SOFTHSM2_LIB);
        ASSERT_TRUE(mLibrary);

        ASSERT_TRUE(mCryptoProvider.Init().IsNone());

        InitTestToken();
    }

    void TearDown() override { ASSERT_TRUE(FS::ClearDir(WORK_DIR "/softhsm/tokens").IsNone()); }

    static constexpr auto mLabel = "iam pkcs11 test slot";
    static constexpr auto mPIN   = "admin";

    void                                    InitTestToken();
    RetWithError<SlotID>                    FindTestToken();
    RetWithError<UniquePtr<SessionContext>> OpenUserSession(bool login = true);

    SlotID                        mSlotID = 0;
    PKCS11Manager                 mManager;
    SharedPtr<LibraryContext>     mLibrary;
    crypto::MbedTLSCryptoProvider mCryptoProvider;

    StaticAllocator<Max(2 * sizeof(pkcs11::PKCS11RSAPrivateKey), sizeof(pkcs11::PKCS11ECDSAPrivateKey),
        2 * sizeof(crypto::x509::Certificate) + sizeof(crypto::x509::CertificateChain)
            + 2 * sizeof(pkcs11::PKCS11RSAPrivateKey))>
        mAllocator;
};

void PKCS11Test::InitTestToken()
{
    Error err = ErrorEnum::eNone;

    Tie(mSlotID, err) = FindTestToken();

    if (err.Is(ErrorEnum::eNotFound)) {
        constexpr auto cDefaultSlotID = 0;

        ASSERT_TRUE(mLibrary->InitToken(cDefaultSlotID, mPIN, mLabel).IsNone());

        Tie(mSlotID, err) = FindTestToken();
        ASSERT_TRUE(err.IsNone());

        UniquePtr<SessionContext> session;

        Tie(session, err) = mLibrary->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
        ASSERT_TRUE(err.IsNone());

        ASSERT_TRUE(session->Login(CKU_SO, mPIN).IsNone());
        ASSERT_TRUE(session->InitPIN(mPIN).IsNone());
        ASSERT_TRUE(session->Logout().IsNone());
    }
}

RetWithError<SlotID> PKCS11Test::FindTestToken()
{
    constexpr auto cMaxSlotListSize = 10;

    StaticArray<SlotID, cMaxSlotListSize> slotList;

    auto err = mLibrary->GetSlotList(true, slotList);
    if (!err.IsNone()) {
        return {0, err};
    }

    for (const auto id : slotList) {
        TokenInfo tokenInfo;

        err = mLibrary->GetTokenInfo(id, tokenInfo);
        if (!err.IsNone()) {
            return {0, err};
        }

        if (tokenInfo.mLabel == mLabel) {
            return {id, ErrorEnum::eNone};
        }
    }

    return {0, ErrorEnum::eNotFound};
}

RetWithError<UniquePtr<SessionContext>> PKCS11Test::OpenUserSession(bool login)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session;

    Tie(session, err) = mLibrary->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
    if (!err.IsNone() || !session) {
        return {nullptr, err};
    }

    if (login) {
        err = session->Login(CKU_USER, mPIN);
        if (!err.IsNone()) {
            return {nullptr, err};
        }
    }

    return session;
}

Error ReadFile(const char* filename, Array<uint8_t>& array)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        return ErrorEnum::eFailed;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    Error err = array.Resize(size);
    if (!err.IsNone()) {
        return ErrorEnum::eFailed;
    }

    if (!file.read(reinterpret_cast<char*>(array.Get()), size)) {
        return ErrorEnum::eFailed;
    }

    return ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Tests
 **********************************************************************************************************************/

TEST_F(PKCS11Test, PrintTestTokenInfo)
{
    LibInfo libInfo;
    ASSERT_TRUE(mLibrary->GetLibInfo(libInfo).IsNone());
    LOG_INF() << "Lib Info: " << libInfo;

    SlotInfo slotInfo;

    ASSERT_TRUE(mLibrary->GetSlotInfo(mSlotID, slotInfo).IsNone());
    LOG_INF() << "Test Slot Info: " << slotInfo;

    TokenInfo tokenInfo;

    ASSERT_TRUE(mLibrary->GetTokenInfo(mSlotID, tokenInfo).IsNone());
    LOG_INF() << "Test Token Info: " << tokenInfo;
}

TEST_F(PKCS11Test, Login)
{
    constexpr auto cBadPIN = "user";

    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session;

    Tie(session, err) = mLibrary->OpenSession(mSlotID, CKF_RW_SESSION | CKF_SERIAL_SESSION);
    ASSERT_TRUE(err.IsNone() && session);

    // Login OK
    ASSERT_TRUE(session->Login(CKU_USER, mPIN).IsNone());
    ASSERT_TRUE(session->Logout().IsNone());

    // Login NOK
    ASSERT_FALSE(session->Login(CKU_USER, cBadPIN).IsNone());
}

TEST_F(PKCS11Test, SessionInfo)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session;
    SessionInfo               sessionInfo;

    Tie(session, err) = OpenUserSession();
    ASSERT_TRUE(err.IsNone());

    ASSERT_TRUE(session->GetSessionInfo(sessionInfo).IsNone());

    EXPECT_EQ(sessionInfo.slotID, mSlotID);
    EXPECT_EQ(sessionInfo.state & CKS_RW_USER_FUNCTIONS, CKS_RW_USER_FUNCTIONS);
    EXPECT_EQ(sessionInfo.flags & (CKF_RW_SESSION | CKF_SERIAL_SESSION), CKF_RW_SESSION | CKF_SERIAL_SESSION);
    EXPECT_EQ(sessionInfo.ulDeviceError, CKR_OK);
}

TEST_F(PKCS11Test, CreateMultipleSessions)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session1, session2, session3;

    Tie(session1, err) = OpenUserSession(true);
    ASSERT_TRUE(err.IsNone());

    // double login failed
    Tie(session2, err) = OpenUserSession(true);
    ASSERT_FALSE(err.IsNone());

    // open remaining sessions, but dont log in
    Tie(session2, err) = OpenUserSession(false);
    ASSERT_TRUE(err.IsNone());

    Tie(session3, err) = OpenUserSession(false);
    ASSERT_TRUE(err.IsNone());
}

TEST_F(PKCS11Test, GenerateRSAKeyPairWithLabel)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session1, session2;

    Tie(session1, err) = OpenUserSession(true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(*session1, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // check key exists in a new session
    Tie(session2, err) = OpenUserSession(false);
    ASSERT_TRUE(err.IsNone());

    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    CK_BBOOL                                             cTrue = CK_TRUE;

    templ.EmplaceBack(CKA_TOKEN, Array<uint8_t>(&cTrue, sizeof(cTrue)));

    ASSERT_TRUE(session2->FindObjects(templ, objects).IsNone());
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPrivHandle()));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPubHandle()));

    // remove key
    err = Utils(*session1, mCryptoProvider, mAllocator).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    ASSERT_TRUE(session2->FindObjects(templ, objects).IsNone());
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPrivHandle())));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPubHandle())));
}

TEST_F(PKCS11Test, GenerateECDSAKeyPairWithLabel)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session1, session2;

    Tie(session1, err) = OpenUserSession(true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err)
        = Utils(*session1, mCryptoProvider, mAllocator).GenerateECDSAKeyPairWithLabel(id, mLabel, EllipticCurve::eP384);
    ASSERT_TRUE(err.IsNone());

    // check key exists in a new session
    Tie(session2, err) = OpenUserSession(false);
    ASSERT_TRUE(err.IsNone());

    StaticArray<ObjectAttribute, cObjectAttributesCount> templ;
    StaticArray<ObjectHandle, cKeysPerToken>             objects;
    CK_BBOOL                                             cTrue = CK_TRUE;

    templ.EmplaceBack(CKA_TOKEN, Array<uint8_t>(&cTrue, sizeof(cTrue)));

    ASSERT_TRUE(session2->FindObjects(templ, objects).IsNone());
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPrivHandle()));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Contains(key.GetPubHandle()));

    // remove key
    err = Utils(*session1, mCryptoProvider, mAllocator).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    ASSERT_TRUE(session2->FindObjects(templ, objects).IsNone());
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPrivHandle())));
    ASSERT_THAT(std::vector<ObjectHandle>(objects.begin(), objects.end()), Not(Contains(key.GetPubHandle())));
}

TEST_F(PKCS11Test, FindPrivateKey)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session;

    Tie(session, err) = OpenUserSession(true);
    ASSERT_TRUE(err.IsNone());

    // generate key
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    PrivateKey key;

    Tie(key, err) = Utils(*session, mCryptoProvider, mAllocator).GenerateRSAKeyPairWithLabel(id, mLabel, 2048);
    ASSERT_TRUE(err.IsNone());

    // find PrivateKey
    PrivateKey foundKey;
    Tie(foundKey, err) = Utils(*session, mCryptoProvider, mAllocator).FindPrivateKey(id, mLabel);
    ASSERT_TRUE(err.IsNone());

    ASSERT_EQ(key.GetPrivHandle(), foundKey.GetPrivHandle());
    ASSERT_EQ(key.GetPubHandle(), foundKey.GetPubHandle());
    ASSERT_TRUE(key.GetPrivKey()->GetPublic().IsEqual(foundKey.GetPrivKey()->GetPublic()));

    // remove key
    err = Utils(*session, mCryptoProvider, mAllocator).DeletePrivateKey(key);
    ASSERT_TRUE(err.IsNone());

    // check key doesn't exist anymore
    Tie(foundKey, err) = Utils(*session, mCryptoProvider, mAllocator).FindPrivateKey(id, mLabel);
    ASSERT_EQ(err, ErrorEnum::eNotFound);
}

TEST_F(PKCS11Test, ImportCertificate)
{
    Error                     err = ErrorEnum::eNone;
    UniquePtr<SessionContext> session;

    Tie(session, err) = OpenUserSession(true);
    ASSERT_TRUE(err.IsNone());

    // import certificate
    uuid::UUID id;

    Tie(id, err) = uuid::StringToUUID("08080808-0404-0404-0404-121212121212");
    ASSERT_TRUE(err.IsNone());

    StaticArray<uint8_t, crypto::cCertDERSize> derBlob;
    crypto::x509::Certificate                  caCert;

    ASSERT_TRUE(ReadFile(WORK_DIR "/certificates/ca.cer.der", derBlob).IsNone());
    ASSERT_TRUE(mCryptoProvider.DERToX509Cert(derBlob, caCert).IsNone());

    ASSERT_TRUE(Utils(*session, mCryptoProvider, mAllocator).ImportCertificate(id, mLabel, caCert).IsNone());

    // check certificate exist
    bool hasCertificate = false;

    Tie(hasCertificate, err)
        = Utils(*session, mCryptoProvider, mAllocator).HasCertificate(caCert.mIssuer, caCert.mSerial);
    ASSERT_TRUE(err.IsNone());
    ASSERT_TRUE(hasCertificate);

    // delete certificate
    err = Utils(*session, mCryptoProvider, mAllocator).DeleteCertificate(id, mLabel);
    ASSERT_TRUE(err.IsNone());

    // check certificate doesn't exist
    Tie(hasCertificate, err)
        = Utils(*session, mCryptoProvider, mAllocator).HasCertificate(caCert.mIssuer, caCert.mSerial);
    ASSERT_TRUE(err.IsNone());
    ASSERT_FALSE(hasCertificate);
}

TEST_F(PKCS11Test, GenPIN)
{
    static constexpr auto cTestPINsNum = 1000;
    static constexpr auto cPINSize     = 20;

    std::vector<StaticString<cPINSize>> pins;

    for (int i = 0; i < cTestPINsNum; i++) {
        StaticString<cPINSize> pin;

        ASSERT_TRUE(GenPIN(pin).IsNone());

        pins.push_back(pin);
    }

    // check there is no equal PINs
    std::sort(pins.begin(), pins.end());

    StaticString<cPINSize> prevPIN;

    for (const auto& pin : pins) {
        ASSERT_NE(pin, prevPIN);
    }
}

} // namespace pkcs11
} // namespace aos
