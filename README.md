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

#### Сборка и запуск в докере

```bash
docker build -t backup-restore-tests .
docker run --rm backup-restore-tests
```

#### Без Докера

```bash
mkdir build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

или

```bash
mkdir -p bin
g++ tests.cpp -lgtest -o ./bin/tests
./bin/tests
```

