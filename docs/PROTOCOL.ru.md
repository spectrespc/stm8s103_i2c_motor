# Спецификация протокола I²C — контроллер двигателей STM8S103

> 🇬🇧 [English version](PROTOCOL.md)

## Параметры шины
- Скорость: стандартный режим (100 кГц)
- Адрес ведомого: 0x60 (7 бит)
- Адрес записи: 0xC0, адрес чтения: 0xC1
- Внешние подтягивающие резисторы: 4.7 кОм на SDA/SCL

## Формат транзакции
- **Только запись**: `[START][0xC0][CMD][MOTOR][PARAM...][STOP]`
- **Запись+чтение**: `[START][0xC0][CMD][MOTOR][REPEATED START][0xC1][DATA...][!ACK][STOP]`

## Справочник команд

| Команда | Запись (payload) | Чтение (payload) | Описание |
|-------|-------------|--------------|-------------|
| 0x01 | [cmd][motor][steps_hi][steps_lo] | — | Переместить двигатель на N шагов (int16 BE, дополнительный код) |
| 0x02 | [cmd][motor][spd_hi][spd_lo] | — | Задать скорость (uint16 BE, шагов/с, макс. 5000) |
| 0x03 | [cmd][motor] | — | Остановить двигатель |
| 0x04 | [cmd][motor] | [pos_0..pos_3] | Запрос текущей позиции (int32 BE) |
| 0x05 | [cmd][motor] | [status] | Запрос состояния (0=IDLE, 1=RUNNING, 2=ERROR) |
| 0x06 | [cmd] | — | Программный сброс контроллера |
| 0xFF | [cmd] | [maj][min][pat] | Получить версию прошивки (3 байта) |

## Подробные примеры команд

### Запрос версии
```bash
i2ctransfer -y 7 w1@0x60 0xFF r3
# Response: 0x02 0x00 0x00 (version 2.0.0)
```

### Задать скорость
```bash
# Set focus motor speed to 500 steps/sec
i2ctransfer -y 7 w4@0x60 0x02 0x00 0x01 0xF4
```

### Перемещение двигателя
```bash
# Move focus motor +100 steps
i2ctransfer -y 7 w4@0x60 0x01 0x00 0x00 0x64
```

### Получить позицию
```bash
# Get focus motor position
i2ctransfer -y 7 w2@0x60 0x04 0x00 r4
# Response: [pos_0..pos_3] (int32 BE)
```

### Получить состояние
```bash
# Get zoom motor status
i2ctransfer -y 7 w2@0x60 0x05 0x01 r1
# Response: 0x00 (IDLE), 0x01 (RUNNING) or 0x02 (ERROR)
```

### Остановить двигатель
```bash
# Stop zoom motor
i2ctransfer -y 7 w2@0x60 0x03 0x01
```

### Программный сброс
```bash
# Reset controller
i2ctransfer -y 7 w1@0x60 0x06
```

## Обработка ошибок
- Неизвестные команды (включая удалённую 0x07) игнорируются (без NACK)
- Ошибки I2C (AF/BERR/OVR) → автоматический сброс модуля
- Таймаут 2 с без активности → сброс модуля