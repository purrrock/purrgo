
# Алгоритм президента

## Модуль импорта``` js
импортировать шифр из @system.cipher
```## API

### `да`
<метод объявления><pre>
(варианты: {
  действие: строка,
  текст: строка,
  ключ: строка,
  преобразование?: строка,
  iv?: строка,
  ivOffset?: число,
  ivLen?: номер
  }): Promise&lt;{ text: string }>
</pre></decl>

`опции`:
- «действие»: тип шифрования и дешифрования, два необязательных значения: «зашифровать»: шифрование, «расшифровать»: дешифрование;
- `текст`: текстовое правило, которое необходимо зашифровать или расшифровать. Текст, текст, текст, ошибочной расшифровки, должен представлять собой фрагмент двойного значения, закодированный с помощью `base64`;
- `ключ`: ключ, эквивалентный для шифрования или дешифрования, строка, созданная после кодирования `base64`. Ключ должен быть уменьшен на $16$ с помощью `bsae64`;
- AES/CBC/PKCS5Padding. AES Padding Дополнительные параметры заполнения:
  - `''PKCS5Padding''`
  - `''PKCS7Padding''`
  - `''NoPadding''`
  - `''OneAndZerosPadding''`
  - `'ZerosAndLenPadding''`
  - `'ZerosPadding'`
- `iv`: начальный вектор для шифрования и дешифрования AES, строка, закодированная Base64, значением по умолчанию является значение поля `key`;
- `ivOffset`: начальное размещение вектора для шифрования и дешифрования AES, значение по умолчанию — $0$;
- `ivLen`: AES, значение по умолчанию — $16$;

::: подробный пример кода``` js
let SignKey = "TkQRXv9xfAU65sxGmx4Xz2tQP7fwwdyxAGIZ9HMtc+c="

асинхронная функция AesTest() {
  const encrypt = ждут cipher.aes({
    действие: «зашифровать»,
    текст: "это тестовый проект!",
    ключ: знакКей,
    iv: "MTIzNDU2NzgxMjM0NTY3OA==",
    преобразование: «AES/CBC/ZerosAndLenPadding»,
    ivOffset: 0,
    ivLen: 16
  })
  console.log(`зашифровать текст: ${encrypt.text}`)

  const decrypt = ждут cipher.aes({
    действие: «расшифровать»,
    текст: шифровать.текст,
    ключ: знакКей,
    iv: "MTIzNDU2NzgxMjM0NTY3OA==",
    преобразование: «AES/CBC/ZerosAndLenPadding»,
    ivOffset: 0,
    ivLen: 16
  })
  console.log(`расшифровать текст: ${decrypt.text}`)
}

AesTest() //Печать зашифрованного и расшифрованного текста, вывод на консоль
// зашифровать текст: yI4dWJzQNCQfXq5P8du1dtYWZuBvbl9F9Vh15Fh9qjg=
// расшифровка текста: это тестовый проект!
```:::

### `rsa`
<метод объявления><pre>
(варианты: {
  действие: строка,
  текст: строка,
  ключ: строка,
  преобразование?: строка
}): Promise&lt;{ text: string }>
</pre></decl>

Шифрование и дешифрование `rsa`, функции поля параметра `options`:
- «действие»: тип шифрования и дешифрования, два необязательных значения: «зашифровать»: шифрование, «расшифровать»: дешифрование;
- `текст`: текстовое правило, которое необходимо зашифровать или расшифровать. Текстовое Base64;
- `key`: ключ `RSA`, строка, сгенерированная после кодирования `base64`, `key` — это открытый ключ при шифровании, а `key` — закрытый ключ при расшифровке;
- «RSA/None/OAEP с SHA-256 и дополнением MGF1». ЮАР:
  - `'PKCS_v15andMGF1Padding'`
  - `'OAEPwithMD5andMGF1Padding''`
  - `'OAEPwithSHA-1 и MGF1Padding''`
  - `'OAEPwithSHA-256 и MGF1Padding''`

::: подробный пример кода``` js
пусть publicKey =
  'MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCirfSt9f49F/BtPqextDlyoUEQ' +
  'qN+NUNxkYB5DY4FmJuI0gQSaK8hlGvnoA5T/setTGylHn95/PPTl5hW+riYtWaKfM' +
  'CXI2scstXA0S5vcYfc9917tRsrFzrDfJW+WD/HmmcvgI6rcbivokDikep3gVX0df' +
  'ktYtsAs158kMs4bBpwIDAQAB'

пусть частныйКей =
  'MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAKKt9K31/j0X8G0+' +
  'p7G0OXKhQRCo341Q3GRgHkNjgWYm4jSBBJoryGUa+egDlP+x5MbKUef3n889OXmF' +
  'b6uJi1Zop8wJcjaxyy1cDRLm9xh9z33Xu1GysXOsN8lb5YP8eaZy+AjqtxuK+iQO' +
  'KR6neBVfR1+S1i2wCzXnyQyzhsGnAgMBAAECgYAuH23w6H7FqYTkJFB9RKDJDEkb' +
  'RRXkxhlGaC4MYyjr4nhd9Hpuj51IdSaHjoRvHmvDpNcmEoH/ytcBykBH/T5As68M' +
  'L1OmzuJsD3BYMZpOOSFC9m7o6VMRf/T/ZTG6EDMtQekxlBV66QpiFmhQMjDs3jJY' +
  'TyR3OnZN9BWNBNotWQJBAOnLupMT53HbFtw9vCRtVgAJ8JFjL4ZzYzrHj4mloKF3' +
  'P/r6faYUjgULoaHiD+BZB/Avru2h74Ghhr26CD3gMR0CQQCyIXzjSCrQiyCEdg1I' +
  '//IWLAALsfVITrlCN0rVeMkjTbc0KFEDUKG9y6MGAGX4AJNnos7y+zLpi6PcgwlU' +
  'zWaTAkBx5+fRVK88n5uhrkpODR8LYcxdaU+sV+eOqc/bJmD+ihuX+JbjJbyT5LjZ' +
  'IETP71CYywKVMIJ6S/JT2aFOVD5ZAkEAsfqFtu2fYbjw54iwY3TfpEmYThcj9Xg6' +
  '4C8wxTQm+/AlkaaKs144DNPPciqpt26T2WOxlNNqHjFYqvX+N832owJAaM5d4x2a' +
  'SDfC5GQFNfZ3WjATXkDE86q3m/88RBFFy8fWByyGiXtp4z5LCtMzI63X3ao0asVK' +
  'mjZxB+T+lMqa3w=='

асинхронная функция rsaTest() {
  const res = ожидайте cipher.rsa({
    действие: «зашифровать»,
    текст: "это тест RSA.",
    ключ: публичный ключ,
    преобразование: «RSA/None/OAEPwithSHA-256andMGF1Padding»
  })
  console.log(`зашифровать текст: ${res.text}`)

  const decrypt = ждут cipher.rsa({
    действие: «расшифровать»,
    текст: res.text,
    ключ: частныйКей,
    преобразование: «RSA/None/OAEPwithSHA-256andMGF1Padding»
  })
  console.log(`расшифровать текст: ${decrypt.text}`)
}

rsaTest() // Печать зашифрованного и расшифрованного текста, пример вывода на консоль
// зашифровать текст: FF+4R3iJ9pjeozZ6/Oulz9LUBH/uGQbIesJ7JbYRWvxGIHpJKNiEB+4MT/JcKs8ddN/ZQ4ts+YWMgUeglRBugR x+T4kqq0rKBdQrYdiMP58deCViSJjXJS+joPppwLDPL1Lg0VxpW89B+gA1jfC+9N8tvEHPhcX+nF8uAKRcW0M=
// расшифровываем текст: это тест RSA.
```:::

### `знак`
<метод объявления><pre>
(варианты: {
  текст: строка,
  ключ: строка,
  алгоритм?: строка,
}): Обещание&lt;{знак: строка }>
</pre></decl>

Подпишите `sign`, функция каждого поля параметра `options`:
- `текст`: содержание загрузки;
- `ключ`: закрытый ключ RSA;
- «SHA256withRSA».
  - `'MD5withRSA'`
  - `'SHA1withRSA'`
  - `'SHA256withRSA''`
  - `'SHA512withRSA''`

::: подробный пример кода``` js
let SignKey1 = "----- НАЧАТЬ ЧАСТНЫЙ КЛЮЧ RSA ----- \ n" +
  "MIIEpAIBAAKCAQEA5hoGkpvqxJdssvqAYuvCWdTRrOdzZyx/ZyMev5Qyt2JKLy1C\n" +
  "7DuKrFGF5T5BDxN81o/OK+AQ6G1ASmwWfv5C1mk7sv6/glibPt9Gyr1OFMxviauy\n" +
  "ZMF8sgHVGkFyy1GsCsaM9anT1OEPoNeqrTHt+xB3Pq6FdH9RLMVbY0QNem5zv816\n" +
  "Hb6AJvMSnbGqMdd9fI1ARithrqnr9p+achP+Hc2Pj61PRviKJpFGLzBrU1BgBEbN\n" +
  "hscGRBebn4kTSy8flYau9lnDyLs5yyy0MHKBhot5Ja3tWTKhaqymFyJL2K6gE6Xn\n" +
  "bDAT6YFvo1TE9R7r9y+8prOR8oznJP19yxEWCQIDAQABAOIBAEbolkXvznUuxMyS\n" +
  "7aWOSaItN0A1Qxb0W36JEByxqr9ghsPrCsiJwL5BkSWH/byLoNjuD/btYch+gmVs\n" +
  "0bHo4Of6He+XGaUtcQn6/HHVzI4UQfsG8j6ica7ZabZhnOKTFJVtglriLulXQd2r\n" +
  "GGmvDUtlU5n5Zh70bSuC1hrNCepEMbJWqRZ4dvrdVqZ5RTARD3PYUAiPzwisQF9q\n" +
  "ZPAayyqmDUBReXS71RKRGn47RST+d50fZ3USP1jTAXMxf+X41ml3l7G1zd90IsWL\n" +
  "aIeHIaxi8BVkQogxqfZH8PAzmqtgLEWDfMgWU879qicBW4FB/PoBkP0P6Qlis/50\n" +
  "yY/80UECgYEA+zAkOshLUSJ4MDRMpkpf1WIZABH2lZhhIFw2A/VYnrmCJj3kxJYJ\n" +
  "ELNm82nFVIJGadSarOpownKUteHcJ7Zzv65WoEEZwZBO453I9tL6Fbh64hPp8VdB\n" +
  "4WMvK+0XqhzBL67ehghFNXc9ud4ZIQOXz6KUASxb+Iz0L02iqWIj+RUCgYEA6oJ5\n" +
  "Sh6Ez1lnWDKI5ZEQ1jn+kgcVHObV1o8sB5/5V0/Lihgma+Lpkei333sQsYImWQMD\n" +
  "8BT4JMCpPph5AwM0ZehUF7d2RCtQ+r0A/pUyiXjtMYHDrmAX94zDtf35QUJOL17z\n" +
  "don0weI/vZ71VYX3saa3EvVJLERwpSr0TswfPiUCgYEARLo8D5fwAsjbMPqlwqve\n" +
  "HpOocV3o3JG+KeyAcFRkLjGOh9GD4JLzhOJ45uVS5nv3A4tJGaLPivbTwAaiJ0TV\n" +
  "b3fo5aYemfYr6WV07hXCFvGWvqPG+UhxaxWTOHd/EGFZjvqG1lAVl2B5t7g8O3GH\n" +
  "ESbQ88WXMOFsgKK4OhXceskCgYEA0W/JJvruncg41bn8LRpLsSeGRaBxqKg33jFr\n" +
  "nzuuEd4/54r99WhoNVljrgFYvU+BNAnPYIE5xIkUHcVKffhEuaauQ6gjxWnyHpzh\n" +
  "4Hwa8E/Bdm9v9bH4dauPtl+mVjQDY6cnRHyczPNk/dKTRNgqiMxdwF60BQbym3Ar\n" +
  "VJxUYskCgYA6HWzf+9uHS98Hhr9zW0akjSZbcZclKR53wFMOjE1mFIxp/dC+d6mf\n" +
  "uVcUDTyo/LygzRBA5sd1euBhm5lXPyEHxIHZvwfBhIZWKlCZWlio1UvDbUp1f32u\n" +
  "JMT6q3KeJFJXp7nf5YmrPOKlh1Lm53hiXLSKF/q6Lcnn2lzRD2JDFw==\n" +
  "-----КОНЕЦ ЧАСТНОГО КЛЮЧА RSA-----"

асинхронная функция SignTest() {
  let res = await cipher.sign({
    text: "это тестовый проект знаков.",
    ключ: знакKey1
  })

  console.log(`текст подписи: ${res.sign}`)
}

знакТест()

```:::

### `хэш`
<метод объявления><pre>
(варианты: {
  данные: строка | МассивБуфер,
  алгоритм: строка,
  закодировать?: строка
}): Promise&lt;string | МассивБуфер>
</pre></decl>

`hash`-шифрование, функция каждого поля задаёт `options`:
- `данные`: исходные данные для формирования сводки;
- «алгоритм»: алгоритм дайджеста, необязательные значения: «md5», «sha1», «sha224», «sha256», «sha384», «sha512»;
- `encode`: кодировка и тип возвращаемых данных, значения:
  - `'hex'`: значение по умолчанию, возвращает запрос в шестнадцатеричном формате;
  - `'base64'': возвращаемое значение представляет собой результат шифрования в кодировке Base64;
  - `'arraybuffer'`: ArrayBuffer;

::: подробный пример кода``` js
асинхронная функция md5Test(){
  const res = ожидайте cipher.hash({
    алгоритм: 'md5',
    данные: 'привет'
  })
  консоль.log(рез)
}
md5Test() // Распечатываем сгенерированную сводку и вывод на консоль
// вывод: 5d41402abc4b2a76b9719d911017c592
```:::

### `хмак`
<метод объявления><pre>
(варианты: {
  данные: строка | МассивБуфер,
  алгоритм: строка,
  ключ: строка | МассивБуфер,
  закодировать?: строка
}): Promise&lt;string | МассивБуфер>
</pre></decl>

Используйте алгоритм HMAC для создания кода аутентификации сообщения с ключом. каждое поле параметра `options`:
- `данные`: исходные данные для формирования сводки;
- «алгоритм»: алгоритм, необязательно «md5», «sha1», «sha224», «sha256», «sha384», «sha512»;
- `ключ`: ключ;
- `encode`: кодировка и тип возвращаемых данных, значения:
  - `'hex'`: значение по умолчанию, возвращает запрос в шестнадцатеричном формате;
  - `'base64'': возвращаемое значение представляет собой результат шифрования в кодировке Base64;
  - Arraybuffer: возвращаемое значение имеет тип ArrayBuffer;

::: подробный пример кода``` js
асинхронная функция hmacTest() {
  пусть res = ждут cipher.hmac({
    данные: «привет»,
    алгоритм: 'sha1',
    ключ: '1234567890'
  })
  консоль.log(рез)
}
hmacTest() // Распечатываем сгенерированную сводку и вывод на консоль
// вывод: 6fce0a55cf8bae80e2cf479b50035f773491c5ad
```:::

### `base64Encode` <decl type="(data: string | ArrayBuffer): Promise&lt;string>" метод />

Base64 кодирует входные данные.

### `base64Decode` <decl type="(data: string | ArrayBuffer): Promise&lt;ArrayBuffer>" метод />

Base64 декодирует входные данные.

::: подробный пример кода``` js
асинхронная функция base64Test() {
  const originalData = 'Привет, мир!';
  const encodedData = ждут cipher.base64Encode(originalData); // Закодированные данные

  console.log('Закодированные данные:', encodedData);

const decodedArrayBuffer = ждут cipher.base64Decode(encodedData); // Декодированные данные

  const uint8Array = новый Uint8Array (decodedArrayBuffer);
  пусть decodedData = '';

  for (let i = 0; i < uint8Array.length; i++) {
    decodedData += String.fromCharCode(uint8Array[i]);
  }

  console.log('Декодированные данные:', decodedData);
}

base64Test() //Распечатываем результаты кодирования и декодирования
// Закодированные данные: SGVsbG8sIFdvcmxkIQ==
// Декодированные данные: Привет, мир!
```:::
