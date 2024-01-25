/*
 * Copyright (C) 2024 Renesas Electronics Corporation.
 * Copyright (C) 2024 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <psa/crypto.h>

#include "aos/common/crypto/mbedtls/cryptoprovider.hpp"
#include "aos/common/crypto/mbedtls/drivers/aos/driverwrap.hpp"

namespace aos {
namespace crypto {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::Error MbedTLSCryptoProvider::Init()
{
    auto ret = psa_crypto_init();

    return ret != PSA_SUCCESS ? AOS_ERROR_WRAP(ret) : aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::CreateCSR(
    const aos::crypto::x509::CSR& templ, const aos::crypto::PrivateKeyItf& privKey, aos::Array<uint8_t>& pemCSR)
{
    mbedtls_x509write_csr csr;
    mbedtls_pk_context    key;

    InitializeCSR(csr, key);

    auto cleanupCSR = [&]() {
        mbedtls_x509write_csr_free(&csr);
        mbedtls_pk_free(&key);
    };

    auto ret = SetupOpaqueKey(key, privKey);
    if (!ret.mError.IsNone()) {
        cleanupCSR();

        return ret.mError;
    }

    auto keyID = ret.mValue;

    auto cleanupPSA = [&]() {
        AosPsaRemoveKey(keyID);

        cleanupCSR();
    };

    auto err = SetCSRProperties(csr, key, templ);
    if (err != aos::ErrorEnum::eNone) {
        cleanupPSA();

        return err;
    }

    err = WriteCSRPem(csr, pemCSR);

    cleanupPSA();

    return err;
}

aos::Error MbedTLSCryptoProvider::PEMToX509Certs(
    const aos::Array<uint8_t>& pemBlob, aos::Array<aos::crypto::x509::Certificate>& resultCerts)
{
    mbedtls_x509_crt crt;

    mbedtls_x509_crt_init(&crt);

    int ret = mbedtls_x509_crt_parse(&crt, pemBlob.Get(), pemBlob.Size());
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);

        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_x509_crt* currentCrt = &crt;

    while (currentCrt != nullptr) {
        aos::crypto::x509::Certificate cert;

        auto err = ParseX509Certs(currentCrt, cert);
        if (!err.IsNone()) {
            mbedtls_x509_crt_free(&crt);

            return err;
        }

        err = resultCerts.PushBack(cert);
        if (!err.IsNone()) {
            mbedtls_x509_crt_free(&crt);

            return err;
        }

        currentCrt = currentCrt->next;
    }

    mbedtls_x509_crt_free(&crt);

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::DERToX509Cert(
    const aos::Array<uint8_t>& derBlob, aos::crypto::x509::Certificate& resultCert)
{
    mbedtls_x509_crt crt;

    mbedtls_x509_crt_init(&crt);

    int ret = mbedtls_x509_crt_parse_der(&crt, derBlob.Get(), derBlob.Size());
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);

        return AOS_ERROR_WRAP(ret);
    }

    auto err = ParseX509Certs(&crt, resultCert);

    mbedtls_x509_crt_free(&crt);

    return err;
}

aos::Error MbedTLSCryptoProvider::ASN1EncodeDN(const aos::String& commonName, aos::Array<uint8_t>& result)
{
    mbedtls_asn1_named_data* dn {};

    int ret = mbedtls_x509_string_to_names(&dn, commonName.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    result.Resize(result.MaxSize());
    uint8_t* start = result.Get();
    uint8_t* p     = start + result.Size();

    ret = mbedtls_x509_write_names(&p, start, dn);
    if (ret < 0) {
        mbedtls_asn1_free_named_data_list(&dn);

        return AOS_ERROR_WRAP(ret);
    }

    size_t len = start + result.Size() - p;

    memmove(start, p, len);

    mbedtls_asn1_free_named_data_list(&dn);

    return result.Resize(len);
}

aos::Error MbedTLSCryptoProvider::ASN1DecodeDN(const aos::Array<uint8_t>& dn, aos::String& result)
{
    int                  ret {};
    size_t               len;
    const unsigned char* end        = dn.Get() + dn.Size();
    unsigned char*       currentPos = const_cast<aos::Array<uint8_t>&>(dn).Get();

    if ((ret = mbedtls_asn1_get_tag(&currentPos, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    const unsigned char* sequenceEnd = currentPos + len;

    while (currentPos < sequenceEnd) {
        if ((ret = mbedtls_asn1_get_tag(&currentPos, sequenceEnd, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET))
            != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        const unsigned char* setEnd = currentPos + len;

        if ((ret = mbedtls_asn1_get_tag(&currentPos, setEnd, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE))
            != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        const unsigned char* seqEnd = currentPos + len;

        if ((ret = mbedtls_asn1_get_tag(&currentPos, seqEnd, &len, MBEDTLS_ASN1_OID)) != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        auto oid {aos::Array<uint8_t> {currentPos, len}};

        auto err = GetOIDString(oid, result);
        if (err != aos::ErrorEnum::eNone) {
            return err;
        }

        currentPos += len;

        unsigned char tag = *currentPos;

        if (tag != MBEDTLS_ASN1_UTF8_STRING && tag != MBEDTLS_ASN1_PRINTABLE_STRING) {
            return aos::ErrorEnum::eInvalidArgument;
        }

        if ((ret = mbedtls_asn1_get_tag(&currentPos, seqEnd, &len, tag)) != 0) {
            return AOS_ERROR_WRAP(ret);
        }

        result.Insert(
            result.end(), reinterpret_cast<const char*>(currentPos), reinterpret_cast<const char*>(currentPos) + len);
        result.Append(", ");

        currentPos += len;
    }

    if (!result.IsEmpty()) {
        // Remove the last two characters (", ")
        result.Resize(result.Size() - 2);
    }

    return aos::ErrorEnum::eNone;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

aos::Error MbedTLSCryptoProvider::ParseX509Certs(mbedtls_x509_crt* currentCrt, aos::crypto::x509::Certificate& cert)
{
    GetX509CertData(cert, currentCrt);

    return GetX509CertExtensions(cert, currentCrt);
}

void MbedTLSCryptoProvider::GetX509CertData(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    cert.mSubject.Resize(crt->subject_raw.len);
    memcpy(cert.mSubject.Get(), crt->subject_raw.p, crt->subject_raw.len);

    cert.mIssuer.Resize(crt->issuer_raw.len);
    memcpy(cert.mIssuer.Get(), crt->issuer_raw.p, crt->issuer_raw.len);

    cert.mSerial.Resize(crt->serial.len);
    memcpy(cert.mSerial.Get(), crt->serial.p, crt->serial.len);
}

aos::Error MbedTLSCryptoProvider::GetX509CertExtensions(aos::crypto::x509::Certificate& cert, mbedtls_x509_crt* crt)
{
    mbedtls_asn1_buf      buf = crt->v3_ext;
    mbedtls_asn1_sequence extns;

    auto ret = mbedtls_asn1_get_sequence_of(
        &buf.p, buf.p + buf.len, &extns, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    mbedtls_asn1_sequence* next = &extns;

    while (next != nullptr) {
        size_t tagLen {};

        auto err = mbedtls_asn1_get_tag(&(next->buf.p), next->buf.p + next->buf.len, &tagLen, MBEDTLS_ASN1_OID);
        if (err != 0) {
            return AOS_ERROR_WRAP(err);
        }

        if (!memcmp(next->buf.p, MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER, tagLen)) {
            unsigned char* p = next->buf.p + tagLen;
            err = mbedtls_asn1_get_tag(&p, p + next->buf.len - 2 - tagLen, &tagLen, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            err = mbedtls_asn1_get_tag(&p, p + next->buf.len - 2, &tagLen, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            cert.mSubjectKeyId.Resize(tagLen);
            memcpy(cert.mSubjectKeyId.Get(), p, tagLen);

            if (!cert.mAuthorityKeyId.IsEmpty()) {
                break;
            }
        }

        if (!memcmp(next->buf.p, MBEDTLS_OID_AUTHORITY_KEY_IDENTIFIER, tagLen)) {
            unsigned char* p = next->buf.p + tagLen;
            size_t         len;

            err = mbedtls_asn1_get_tag(&p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_OCTET_STRING);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            if (*p != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
                return AOS_ERROR_WRAP(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
            }

            err = mbedtls_asn1_get_tag(
                &p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            if (*p != (MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0)) {
                return AOS_ERROR_WRAP(MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
            }

            err = mbedtls_asn1_get_tag(&p, next->buf.p + next->buf.len, &len, MBEDTLS_ASN1_CONTEXT_SPECIFIC | 0);
            if (err != 0) {
                return AOS_ERROR_WRAP(err);
            }

            cert.mAuthorityKeyId.Resize(len);
            memcpy(cert.mAuthorityKeyId.Get(), p, len);

            if (!cert.mSubjectKeyId.IsEmpty()) {
                break;
            }
        }

        next = next->next;
    }

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::GetOIDString(aos::Array<uint8_t>& oid, aos::String& result)
{
    mbedtls_asn1_buf oidBuf;
    oidBuf.p   = oid.Get();
    oidBuf.len = oid.Size();

    const char* shortName {};

    int ret = mbedtls_oid_get_attr_short_name(&oidBuf, &shortName);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    if (shortName == nullptr) {
        return aos::ErrorEnum::eNone;
    }

    result.Append(shortName).Append("=");

    return aos::ErrorEnum::eNone;
}

void MbedTLSCryptoProvider::InitializeCSR(mbedtls_x509write_csr& csr, mbedtls_pk_context& pk)
{
    mbedtls_x509write_csr_init(&csr);
    mbedtls_pk_init(&pk);

    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
}

aos::Error MbedTLSCryptoProvider::SetCSRProperties(
    mbedtls_x509write_csr& csr, mbedtls_pk_context& pk, const aos::crypto::x509::CSR& templ)
{
    mbedtls_x509write_csr_set_key(&csr, &pk);

    aos::StaticString<aos::crypto::cCertSubjSize> subject;
    auto                                          err = ASN1DecodeDN(templ.mSubject, subject);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    auto ret = mbedtls_x509write_csr_set_subject_name(&csr, subject.CStr());
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    err = SetCSRAlternativeNames(csr, templ);
    if (err != aos::ErrorEnum::eNone) {
        return err;
    }

    return SetCSRExtraExtensions(csr, templ);
}

aos::Error MbedTLSCryptoProvider::SetCSRAlternativeNames(
    mbedtls_x509write_csr& csr, const aos::crypto::x509::CSR& templ)
{
    mbedtls_x509_san_list   sanList[aos::crypto::cAltDNSNamesCount];
    aos::crypto::x509::CSR& tmpl         = const_cast<aos::crypto::x509::CSR&>(templ);
    size_t                  dnsNameCount = tmpl.mDNSNames.Size();

    for (size_t i = 0; i < tmpl.mDNSNames.Size(); i++) {
        sanList[i].node.type                      = MBEDTLS_X509_SAN_DNS_NAME;
        sanList[i].node.san.unstructured_name.tag = MBEDTLS_ASN1_IA5_STRING;
        sanList[i].node.san.unstructured_name.len = tmpl.mDNSNames[i].Size();
        sanList[i].node.san.unstructured_name.p   = reinterpret_cast<unsigned char*>(tmpl.mDNSNames[i].Get());

        sanList[i].next = (i < dnsNameCount - 1) ? &sanList[i + 1] : nullptr;
    }

    return AOS_ERROR_WRAP(mbedtls_x509write_csr_set_subject_alternative_name(&csr, sanList));
}

aos::Error MbedTLSCryptoProvider::SetCSRExtraExtensions(mbedtls_x509write_csr& csr, const aos::crypto::x509::CSR& templ)
{
    for (const auto& extension : templ.mExtraExtensions) {
        const char*          oid      = extension.mId.CStr();
        const unsigned char* value    = extension.mValue.Get();
        size_t               oidLen   = extension.mId.Size();
        size_t               valueLen = extension.mValue.Size();

        int ret = mbedtls_x509write_csr_set_extension(&csr, oid, oidLen, 0, value, valueLen);
        if (ret != 0) {
            return AOS_ERROR_WRAP(ret);
        }
    }

    return aos::ErrorEnum::eNone;
}

aos::Error MbedTLSCryptoProvider::WriteCSRPem(mbedtls_x509write_csr& csr, aos::Array<uint8_t>& pemCSR)
{
    unsigned char buffer[4096];
    auto          ret = mbedtls_x509write_csr_pem(&csr, buffer, sizeof(buffer), nullptr, nullptr);
    if (ret != 0) {
        return AOS_ERROR_WRAP(ret);
    }

    pemCSR.Resize(strlen(reinterpret_cast<const char*>(buffer)) + 1);
    memcpy(pemCSR.Get(), buffer, pemCSR.Size());

    return aos::ErrorEnum::eNone;
}

aos::RetWithError<mbedtls_svc_key_id_t> MbedTLSCryptoProvider::SetupOpaqueKey(
    mbedtls_pk_context& pk, const aos::crypto::PrivateKeyItf& privKey)
{
    auto statusAddKey = AosPsaAddKey(privKey);
    if (!statusAddKey.mError.IsNone()) {
        return statusAddKey;
    }

    auto ret = mbedtls_pk_setup_opaque(&pk, statusAddKey.mValue);
    if (ret != 0) {
        AosPsaRemoveKey(statusAddKey.mValue);

        return aos::RetWithError<mbedtls_svc_key_id_t>(statusAddKey.mValue, AOS_ERROR_WRAP(ret));
    }

    return aos::RetWithError<mbedtls_svc_key_id_t>(statusAddKey.mValue, aos::ErrorEnum::eNone);
}

} // namespace crypto
} // namespace aos
