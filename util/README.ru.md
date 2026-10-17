# motorctl — утилита I2C для контроллера двигателей STM8S103

> 🇬🇧 [English version](README.md)

Эта утилита позволяет управлять контроллером двух шаговых двигателей STM8S103 по I2C.

## Сборка

```bash
cd util
make
```

## Установка

```bash
make install
```

Бинарный файл будет установлен в `/usr/local/bin/motorctl`.

## Использование

```
Usage: motorctl [OPTIONS] COMMAND [ARGS]
Options:
  -b BUS    I2C bus number (default: 7)
  -a ADDR  I2C address (default: 0x60)
  -h        Show this help message

Commands:
  version        Get firmware version
  speed <motor> <speed>  Set motor speed
  move <motor> <steps>  Move motor by steps
  position <motor>    Get motor position
  status <motor>  Get motor status
  stop <motor>    Stop motor
  reset          Reset controller

Motor names: focus, zoom
```

## Примеры

Получить версию прошивки:
```bash
motorctl version
```

Задать скорость двигателя focus 500 шагов/с:
```bash
motorctl speed focus 500
```

Переместить двигатель zoom на 100 шагов:
```bash
motorctl move zoom 100
```

Получить позицию двигателя focus:
```bash
motorctl position focus
```

Получить состояние двигателя zoom:
```bash
motorctl status zoom
```

Остановить двигатель focus:
```bash
motorctl stop focus
```

Сбросить контроллер:
```bash
motorctl reset
```

## Требования

- Система Linux с поддержкой I2C
- Права доступа к I2C (обычно требуются права root или членство в группе i2c)
- Подключённый контроллер двигателей STM8S103 по адресу I2C 0x60