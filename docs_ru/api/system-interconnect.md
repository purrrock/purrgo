
# соединение устройства

## Модуль импорта``` тс
импортировать межсоединение из @system.interconnect
```##Определение интерфейса

### `instance` <decl type="(options: {package: string, Fingerprint: string}): метод Connect"/>

экземпляр экземпляра [`Connect`](#connect-interface)```js
const Connect = Interconnect.instance({
  пакет: «com.xxxx.xxx»,
  отпечаток пальца: «ххххх»
})
```- пакет: имя пакета официального приложения.
- отпечаток пальца: информация об отпечатке пальца, которая должна соответствовать информации об отпечатке пальца, рабочей при создании соединений мобильным приложением.

## Интерфейс `Подключиться`

### `onopen` <decl type="?: () => void" set />

Используется для указания обратного вызова при размыкании соединения.```js
Connect.onopen = () => {
  console.info("onopen")
}
```### `onclose` <decl type="?: () => void" set />

Используется для указания обратного вызова при замыкании соединения.```js
Connect.onclose = () => {
  console.info("onclose")
}
```### `onerror` <decl type="?: () => void" set />

Используется для указания обратного вызова после сбоя соединения.```js
Connect.onerror = (данные: любые) => {
  console.info("ошибка", данные)
}
```### `onmessage` <decl type="?: () => " set />

Используется для указания обратного вызова для получения данных из местных приложений.```js
Connect.onmessage = (msg => {
  если (msg.isFileType) {
    this.msg = "получить файл" + msg.fileUri
  } еще {
    this.msg = "получить текстовое сообщение" + msg.data
  }
})
```### `send` <decl type="(options: {data: Any}): Promise<any>" метод />

Отправить данные в мобильное приложение```js
Connect.send({
  данные: {
    название: «Чжансан»
  }
})
```