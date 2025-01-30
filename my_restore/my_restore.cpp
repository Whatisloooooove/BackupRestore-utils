#include <sys/statvfs.h>

#include <ctime>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace file_sys = std::filesystem;

// Проверка на наличие прав доступа на чтение других пользователей
bool HasCopyPermission(file_sys::path path) {
  file_sys::file_status stat = file_sys::status(path);
  return (stat.permissions() & file_sys::perms::owner_read) ==
         file_sys::perms::none;
}

// Сравнивает директории и при различиях копирует в нужную папку
void CopyFiles(file_sys::path current_file, file_sys::path path_to) {
  if (HasCopyPermission(current_file)) {
    throw std::runtime_error(
        "Нет права на копирование файла в директорию, в которой вы хотите "
        "создать "
        "резервную копию\nПоменяйте права на файлы");
  }

  if (file_sys::is_regular_file(current_file)) {
    if (!file_sys::exists(path_to / current_file.filename())) {
      file_sys::create_directory(path_to);
      file_sys::copy_file(current_file, path_to / current_file.filename(),
                          file_sys::copy_options::none);
      return;
    }

    if (file_sys::last_write_time(current_file) !=
            file_sys::last_write_time(path_to / current_file.filename()) &&
        file_sys::file_size(current_file) !=
            file_sys::file_size(path_to / current_file.filename())) {
      file_sys::copy_file(current_file, path_to / current_file.filename(),
                          file_sys::copy_options::overwrite_existing);
    }
  } else if (file_sys::is_directory(current_file)) {
    if (!file_sys::exists(path_to / current_file.filename())) {
      file_sys::copy(current_file, path_to / current_file.filename(), file_sys::copy_options::recursive);
      return;
    }
    if (file_sys::last_write_time(current_file) !=
        file_sys::last_write_time(path_to / current_file.filename())) {
      for (const auto& component : file_sys::directory_iterator(current_file)) {
        auto comp_name = component.path().filename();
        CopyFiles(component.path(), path_to / current_file.filename());
      }
    }
  }
}

// Выполняет проверку переданных путей
void MyRestore(file_sys::path path_from, file_sys::path path_to) {
  if (!file_sys::exists(path_from) || !file_sys::exists(path_to)) {
    throw std::runtime_error(
        "Одна из переданных директорий не существует.\nПроверьте правильно ли "
        "вы указали путь до нужных вам директорий либо создайте их.");
  }

  if (!file_sys::is_directory(path_from) || !file_sys::is_directory(path_to)) {
    throw std::runtime_error(
        "Один из переданных путей ведёт не к директории.\nПроверьте правильно "
        "ли вы указали путь до нужных вам директорий либо создайте их.");
  }

  file_sys::file_status stat = file_sys::status(path_to);
  if ((stat.permissions() & file_sys::perms::owner_write) ==
      file_sys::perms::none) {
    throw std::runtime_error(
        "Нет права чтобы создать папку внутри директории для бэкапа\nПоменяйте "
        "права на файлы внутри "
        " директории для копирования");
  }

  try {
    for (const auto& component : file_sys::directory_iterator(path_from)) {
      CopyFiles(component.path(), path_to);
    }
  } catch (std::runtime_error& error) {
    throw;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Вы неправильно используете команду." << '\n' << '\n';
    std::cerr << "Формат ввода:" << '\n';
    std::cerr << "./my_restore [path from] [path to]" << '\n' << '\n';
    std::cerr << "Попробуйте снова!" << '\n';
    return 1;
  }

  file_sys::path path_from = argv[1];
  file_sys::path path_to = argv[2];

  try {
    MyRestore(path_from, path_to);
  } catch (std::runtime_error& error) {
    std::cerr << "Упс, кажется, программа завершилась с ошибкой!" << '\n'
              << '\n';
    std::cerr << error.what() << '\n' << '\n';
    std::cerr << "Попробуйте снова после исправления ошибки!" << '\n';
    return 1;
  }
}