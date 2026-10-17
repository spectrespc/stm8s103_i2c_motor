# CHANGELOG

## [1.0.0] — 2026-10-17
### Добавлено
- Первая версия прошивки контроллера двух шаговых двигателей (focus + zoom)
- I²C slave на адресе 0x60 (100 kHz)
- Команды: move, set speed, stop, get position, get status, reset, calibrate, get version
- Сохранение позиций в EEPROM с CRC16
- IWDG watchdog ~500 мс
- Защита от переполнения int32 при накоплении позиции
- host-утилита motorctl в util/
- Документация: HARDWARE.md, PROTOCOL.md в docs/

### Изменено
- Реструктуризация репозитория: исходники разнесены по модулям src/core, src/hal, src/i2c, src/motor
- Артефакты сборки вынесены в build/