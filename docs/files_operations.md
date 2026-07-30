[2m16:00:15.875[0m[32m I[0m [34mjs.go[0m: PurrGo: Запуск проверки чтения ресурсов из .pkg
[2m16:00:15.875[0m[32m I[0m [34mjs.go[0m: Проверка URI [1/4]: pkg://com.purr.go/purrgo.gpx
[2m16:00:15.904[0m[32m I[0m [34mjs.go[0m: [ОШИБКА] Не удалось прочитать pkg://com.purr.go/purrgo.gpx: IO error with uri pkg://com.purr.go/purrgo.gpx
[2m16:00:15.904[0m[32m I[0m [34mjs.go[0m: Проверка URI [2/4]: pkg://com.purr.go/assets/purrgo.gpx
[2m16:00:15.924[0m[32m I[0m [34mjs.go[0m: [ОШИБКА] Не удалось прочитать pkg://com.purr.go/assets/purrgo.gpx: IO error with uri pkg://com.purr.go/assets/purrgo.gpx
[2m16:00:15.924[0m[32m I[0m [34mjs.go[0m: Проверка URI [3/4]: /purrgo.gpx
[2m16:00:15.940[0m[32m I[0m [34mjs.go[0m: [ОШИБКА] Не удалось прочитать /purrgo.gpx: IO error with uri /purrgo.gpx
[2m16:00:15.940[0m[32m I[0m [34mjs.go[0m: Проверка URI [4/4]: /assets/purrgo.gpx
[2m16:00:15.955[0m[32m I[0m [34mjs.go[0m: [ОШИБКА] Не удалось прочитать /assets/purrgo.gpx: IO error with uri /assets/purrgo.gpx
[2m16:00:15.955[0m[32m I[0m [34mjs.go[0m: Проверьте включение файла в итоговый пакет .pkg при сборке

Может быть мы просто неправильно читаем файл, найди в документации пример fs.readText и сравни с нашим кодом