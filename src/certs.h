#pragma once
// TLS certificate and private key for the HTTPS server.
// Self-signed RSA-2048, CN=sailingcomputer.local, valid 10 years from 2026-06-05.
// Browsers will show a cert warning — accept it once; the connection is still encrypted.
//
// To regenerate (e.g. after expiry in 2036):
//   openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 3650 -nodes \
//     -subj "/CN=sailingcomputer.local/O=SailingComputer/C=US"
// Then run scripts/embed_certs.py to convert PEM → C string literals in this file.
//
// The private key is intentionally embedded in firmware — there is no CA trust chain.
// This is acceptable for a LAN-only device where the user accepts the self-signed cert.

static const char server_cert_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDbzCCAlegAwIBAgIUL+sgcOnQ4XZtNXuOElNsKbOreg4wDQYJKoZIhvcNAQEL\n"
    "BQAwRzEeMBwGA1UEAwwVc2FpbGluZ2NvbXB1dGVyLmxvY2FsMRgwFgYDVQQKDA9T\n"
    "YWlsaW5nQ29tcHV0ZXIxCzAJBgNVBAYTAlVTMB4XDTI2MDYwNTE0NTc1OVoXDTM2\n"
    "MDYwMjE0NTc1OVowRzEeMBwGA1UEAwwVc2FpbGluZ2NvbXB1dGVyLmxvY2FsMRgw\n"
    "FgYDVQQKDA9TYWlsaW5nQ29tcHV0ZXIxCzAJBgNVBAYTAlVTMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAr9HyUiIzWcqc8f8CmXV6Ac+VNwNs7XSHD5nF\n"
    "1Kw4/KYYRCvRcICKZYqyRprxEgFps/jgmxUx79adUrCNDyAwGR1UM+UZGq6qZZKs\n"
    "kNvjre9UVdr6mcmeQ/hv1BK9+eSOEXpEZ58L6MvPSMc0PI/Zc5t0dkUdu5x80vPW\n"
    "OaQOqV7Lg7uoOn2n9w6G+GOKpYYr6Lvhe6S0tjMfte2uyxEYoXSU7VycybqS2XZ3\n"
    "VyPpbUYkwwF1bcJMaaEaZBqo9sXikupHG8yZmUHjufjI03TITs76xeoCPpzSXYIH\n"
    "EME8Pq3EJOjB8VIc9x33fhZ+iY/3FsyWlB/V5FLZlxJ3PGGq+wIDAQABo1MwUTAd\n"
    "BgNVHQ4EFgQUKh44+7uk+PuhoD3J5x+CciuEEdQwHwYDVR0jBBgwFoAUKh44+7uk\n"
    "+PuhoD3J5x+CciuEEdQwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEAoMS5Ms+6LUYNcWFOlwHH6qGtkKv6VuCvfe6q72UDK98bG4BgIgGkULC2J+YY\n"
    "OaOAlpa/V+kHCQE2j0x/3r+RNAacug4bijw3FUCudJ1VtEVRdbE28gUq3aEYHyER\n"
    "4cJt+igrH42HLkvkS1OdYiukl4aWuD23DSiMS0ubfvHvFWiRHeJhBRcFiS9I1Mns\n"
    "Xz/Q8L9QPFDu9NO1JLarksny6AKzZ+AfHngQJt7LixLFYn3/ZOZvq7UwwzQfq8mK\n"
    "dt3AKqwPo+n57dnx0rGvoXveNfzmscxWdJmMkWHz9pYFjjRia+dfqFSDyPnho2Cj\n"
    "QsFI3ZrH9l3xYs9nH3kEG2ootA==\n"
    "-----END CERTIFICATE-----\n";

static const char server_key_pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCv0fJSIjNZypzx\n"
    "/wKZdXoBz5U3A2ztdIcPmcXUrDj8phhEK9FwgIplirJGmvESAWmz+OCbFTHv1p1S\n"
    "sI0PIDAZHVQz5RkarqplkqyQ2+Ot71RV2vqZyZ5D+G/UEr355I4RekRnnwvoy89I\n"
    "xzQ8j9lzm3R2RR27nHzS89Y5pA6pXsuDu6g6faf3Dob4Y4qlhivou+F7pLS2Mx+1\n"
    "7a7LERihdJTtXJzJupLZdndXI+ltRiTDAXVtwkxpoRpkGqj2xeKS6kcbzJmZQeO5\n"
    "+MjTdMhOzvrF6gI+nNJdggcQwTw+rcQk6MHxUhz3Hfd+Fn6Jj/cWzJaUH9XkUtmX\n"
    "Enc8Yar7AgMBAAECggEAOAEEkoQBXmMrLexgXBhvBh+ktJLbQiIBmYcOXImFV3ji\n"
    "tuhMPO086E5PLi1Rh746eNDYXp3YO8DdeQbyz7QdjgppFWSu4/BqJ5Oac0QPmIpp\n"
    "DzYC6DSwPI9os6FKsUxLsqaDHoyrYvp0qwaFDZt2lRwZ80hdybJ1kk6IWPD4mJsp\n"
    "MYRsiES2Ic9ix7NnAkL+gdzGlRnFyVRKWrJccwNK7pHp6gWH4rtVXVIeqYGU/Myx\n"
    "+rPdWoaT+9oMSrJry7PCsJxXHitSai0feyAN7lC3vQY0jsRYSzHY5i8iyLjuFIMi\n"
    "Uu6sPvwgZv3gdJ1DIR3dUqLHArWWzvYXkmE5aDbsiQKBgQDo9Z7kEFUH2qjw9sPs\n"
    "htqio5S1I5ohcmrGwN55ZURuWKKezMhTSG+YL0G8nrs+mgbbVrsgXBxB/kSIgeyO\n"
    "nNY4Xh391A5KhbDAaTjpFJOMp8pYr48lt3G9/wZgBOEE3c6XIzxb5Hk66hjVW9Z7\n"
    "i3VAJmJaTQZfOugbT7G8FZ/++QKBgQDBNZhlm2mMRNJ5yzjIPeeDGn+K2HHYSW3J\n"
    "rKerNJcq8GxE60J2UuFUqARmgFz2UAj7ZlQ5zZhMuJti4DDbKezW61wNA0KqE0v0\n"
    "C0DuVyTNbyKu3atpUKBFgEk6Cm0ZIDe5Xr0eBs8kLCcgpNgXLFMItDB0qGmm+NKA\n"
    "auT/iarSkwKBgFM+AUrJMzkdPyTraFMKVPGstiSL1jWBZvoiTNzf/LXZYjKQRjzM\n"
    "M2QT7s9xgML70ttpgHAtucMzElYc5uSG0l1N9DWRIpIqd2ApuTZALgEiq8FI6kO1\n"
    "6yTTDfodhDJy97E13AmR+Ge+4qTKrjdzO7Byhs5xm4dHy0yHC6GDsKoxAoGAOnj3\n"
    "6DhClzr03/tK8f8aI6lPVDvxKF4ApfpkvAGshkhA3BK/CIRIwZAf1M2gVMMgFMWB\n"
    "VAUOxJlSHXhwEMP9c4XDVATalhJE+FS3j+o7rxilTHq1t6e4+Y+7mZ8yKVqAws8T\n"
    "ORUid3YNWWnKJCk77/RofcXCQ9AmFMtFBrkpQ5UCgYAeoGVLSx/S/ywGWsvawnJH\n"
    "FhH6Ab5cBc7Pc58M4gY+UECD2E/s1hRok1mWw6MQ2T8dhpDQgvtW/V/6mBfBK+Eo\n"
    "VF5rcnz3SlmBR15Lru3Y8Ksr/q5Wms7OuKL8b2OVSo9WnMeBga69Z0XT6gr6AG8B\n"
    "f07bpkFrjDzp5oxH7SDyoQ==\n"
    "-----END PRIVATE KEY-----\n";
