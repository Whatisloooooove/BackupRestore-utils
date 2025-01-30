# Утилиты MyBackup и MyRestore

Здесь реализованы необходимые утилиты для резервного копирования и восстановления копии в нужную директорию

# Как запускать

Для непосредственного использования:

### MyBackup

```bash
./bin/my_backup [full/incremental] [path_from] [path_to]
```

### MyRestore

```bash
./bin/my_restore [path_to_copy] [path_to]
```

### Для запуска с тестами

```bash
mkdir -p bin
g++ tests.cpp -lgtest -o ./bin/tests
./bin/tests
```
