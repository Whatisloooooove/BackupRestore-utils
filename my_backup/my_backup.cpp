#include <sys/statvfs.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace file_sys = std::filesystem;

// Фиксирует в файл последний бэкап
void WriteLastFullBackup(file_sys::path path_to) {
  file_sys::path parent_path = path_to.parent_path();
  file_sys::path dir_name = path_to.filename();
  std::ofstream last_full(parent_path.string() + "/last_full.txt");
  last_full << dir_name.string();
  last_full.close();
}

// Проверка на наличие прав доступа на чтение других пользователей
bool HasCopyPermission(file_sys::path path) {
  file_sys::file_status stat = file_sys::status(path);
  return (stat.permissions() & file_sys::perms::owner_read) ==
         file_sys::perms::none;
}

// Обработка full backup
void ProcessFull(file_sys::path path_from, file_sys::path path_to) {
  for (const auto& component :
       file_sys::recursive_directory_iterator(path_from)) {
    if (HasCopyPermission(component.path())) {
      throw std::runtime_error(
          "Нет права на копирование файла в директорию, в которой вы хотите "
          "создать "
          "резервную копию\nПоменяйте права на файлы");
    }

    auto relative_path = std::filesystem::relative(component.path(), path_from);
    auto dest_path = path_to / relative_path;
    if (file_sys::is_regular_file(component)) {
      file_sys::copy_file(component.path(), dest_path,
                          file_sys::copy_options::none);
    } else if (file_sys::is_directory(component)) {
      file_sys::create_directory(dest_path);
    }
  }

  WriteLastFullBackup(path_to);
}

// Чтение последнего full backup
std::string ReadLastFullBackup(file_sys::path path_to) {
  file_sys::path parent_path_to = path_to.parent_path();
  std::string dir_name;
  std::ifstream last_full_file(parent_path_to.string() + "/last_full.txt");
  std::getline(last_full_file, dir_name);
  return dir_name;
}

// Сравнивает директории и при различиях копирует в нужную папку
void CompareDirectorties(file_sys::path current_file,
                         file_sys::path last_backup_file,
                         file_sys::path path_to) {
  if (HasCopyPermission(current_file)) {
    throw std::runtime_error(
        "Нет права на копирование файла в директорию, в которой вы хотите "
        "создать "
        "резервную копию\nПоменяйте права на файлы");
  }
  if (file_sys::is_regular_file(current_file)) {
    file_sys::create_directory(path_to);
    if (!file_sys::exists(last_backup_file)) {
      file_sys::copy_file(current_file, path_to / current_file.filename(),
                          file_sys::copy_options::none);
      return;
    }

    if (file_sys::last_write_time(current_file) !=
            file_sys::last_write_time(last_backup_file) &&
        file_sys::file_size(current_file) !=
            file_sys::file_size(last_backup_file)) {
      file_sys::create_directories(path_to);
      file_sys::copy_file(current_file, path_to / current_file.filename(),
                          file_sys::copy_options::overwrite_existing);
    }
  } else if (file_sys::is_directory(current_file)) {
    if (!file_sys::exists(last_backup_file)) {
      file_sys::copy(current_file, path_to, file_sys::copy_options::recursive);
      return;
    }
    if (file_sys::last_write_time(current_file) !=
        file_sys::last_write_time(last_backup_file)) {
      for (const auto& component : file_sys::directory_iterator(current_file)) {
        auto comp_name = component.path().filename();
        CompareDirectorties(component.path(), last_backup_file / comp_name,
                            path_to / current_file.filename());
      }
    }
  }
}

// Обработка incremental backup
void ProcessIncremental(file_sys::path path_from, file_sys::path path_to) {
  std::string last_full_backup = ReadLastFullBackup(path_to);
  if (last_full_backup.empty()) {
    ProcessFull(path_from, path_to);
    return;
  }
  file_sys::path path_last_full =
      path_to.parent_path().string() + "/" + last_full_backup;
  for (const auto& component : file_sys::directory_iterator(path_from)) {
    auto path_last_full_comp = path_last_full / component.path().filename();
    CompareDirectorties(component.path(), path_last_full_comp, path_to);
  }
}

// Создаёт директорию, в которой будет хранится копия
std::string CreateBackupDir(file_sys::path path_to) {
  std::time_t seconds = std::time(nullptr);
  std::tm now_time;
  localtime_r(&seconds, &now_time);
  std::ostringstream oss;
  oss << std::put_time(&now_time, "%Y-%m-%d-%H-%M-%S");
  file_sys::path dir_name = oss.str();
  file_sys::create_directory(path_to / dir_name);
  return dir_name.string();
}

// Проверяет есть ли свободное пространство на диске для копирования
void CheckFreeSpace(file_sys::path path_from, file_sys::path path_to) {
  uintmax_t size_dir_to = 0;
  for (const auto& entry : file_sys::recursive_directory_iterator(path_from)) {
    if (file_sys::is_regular_file(entry)) {
      size_dir_to += file_sys::file_size(entry);
    }
  }

  file_sys::space_info space_to = file_sys::space(path_to);
  if (size_dir_to > space_to.free) {
    throw std::runtime_error(
        "Нет свободного пространства на диске для копирования\nОчистите "
        "память");
  }
}

// Выполняет проверки переданных путей, размера свободного пространства на диске
// и обрабатывает переданный флаг
void MyBackup(file_sys::path option, file_sys::path path_from,
              file_sys::path path_to) {
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
    CheckFreeSpace(path_from, path_to);

    if (!file_sys::exists(path_to / "last_full.txt")) {
      std::ofstream(path_to / "last_full.txt").close();
    }
    path_to = path_to / CreateBackupDir(path_to);
  } catch (std::runtime_error& error) {
    throw;
  }

  try {
    if (option == "full") {
      ProcessFull(path_from, path_to);
    } else if (option == "incremental") {
      ProcessIncremental(path_from, path_to);
    } else {
      throw std::runtime_error(
          "Передан некорректный флаг\nВыберите или full, или incremental");
    }
  } catch (std::runtime_error& error) {
    throw;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Вы неправильно используете команду." << '\n' << '\n';
    std::cerr << "Формат ввода:" << '\n';
    std::cerr << "./my_backup [full/incremental] [path from] [path to]" << '\n'
              << '\n';
    std::cerr << "Попробуйте снова!" << '\n';
    return 1;
  }

  file_sys::path path_from = argv[2];
  file_sys::path path_to = argv[3];

  try {
    MyBackup(argv[1], path_from, path_to);
  } catch (std::runtime_error& error) {
    std::cerr << "Упс, кажется, программа завершилась с ошибкой!" << '\n'
              << '\n';
    std::cerr << error.what() << '\n' << '\n';
    std::cerr << "Попробуйте снова после исправления ошибки!" << '\n';
    return 1;
  }
}