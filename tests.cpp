#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>

namespace file_sys = std::filesystem;

class BackupTests : public ::testing::Test {
 protected:
  file_sys::path work;
  file_sys::path backup;

  void InitBackupEnvironment() {
    work = file_sys::temp_directory_path() / "test_source";
    backup = file_sys::temp_directory_path() / "test_backup";

    file_sys::create_directory(work);
    file_sys::create_directory(backup);
    file_sys::create_directories(work / "subdir1");
    file_sys::create_directories(work / "subdir1/subdir2");

    std::ofstream(work / "file1.txt") << "Test file 1";
    std::ofstream(work / "subdir1/file2.txt") << "Test file 2";
    std::ofstream(work / "subdir1/subdir2/file3.txt") << "Test file 3";
  }

  void SetUp() override { InitBackupEnvironment(); }

  void TearDown() override {
    file_sys::remove_all(work);
    file_sys::remove_all(backup);
  }

  file_sys::path GetTimeName() {
    std::time_t seconds = std::time(nullptr);
    std::tm* now_time = std::localtime(&seconds);
    std::ostringstream oss;
    oss << std::put_time(now_time, "%Y-%m-%d-%H-%M-%S");
    file_sys::path dir_name = oss.str();
    return dir_name;
  }

  std::string ReadFile(file_sys::path path) {
    std::ifstream file(path);
    std::string text;
    std::getline(file, text);
    return text;
  }

  std::string RunCommand(std::string command) {
    std::array<char, 128> buffer;
    std::string result;

    command += " 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"),
                                                  pclose);
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }
    return result;
  }
};

// Тест на корректную обработку неправильного формата
TEST_F(BackupTests, IncorrectFormatCommand) {
  std::string output =
      RunCommand("./bin/my_backup full " + work.string());
  std::string expected_out =
      "Вы неправильно используете команду.\n\nФормат ввода:\n./my_backup "
      "[full/incremental] [path from] [path to]\n\nПопробуйте "
      "снова!\n";
  EXPECT_EQ(output, expected_out);
}

// Тест, который проверяет коректные ли пути переданы
TEST_F(BackupTests, IncorrectDirectories) {
  std::string output =
      RunCommand("./bin/my_backup full " + work.string() +
                 "/unexested_directory " + backup.string());
  std::string expected_output =
      "Упс, кажется, программа завершилась с ошибкой!\n\nОдна из переданных "
      "директорий не существует.\nПроверьте правильно ли вы указали путь до "
      "нужных вам директорий либо создайте их.\n\nПопробуйте снова после "
      "исправления ошибки!\n";
  EXPECT_EQ(output, expected_output);

  output = RunCommand("./bin/my_backup full " + work.string() +
                      "/file1.txt " + backup.string());
  expected_output =
      "Упс, кажется, программа завершилась с ошибкой!\n\n"
      "Один из переданных путей ведёт не к директории.\nПроверьте правильно "
      "ли вы указали путь до нужных вам директорий либо создайте их."
      "\n\nПопробуйте снова после "
      "исправления ошибки!\n";
  EXPECT_EQ(output, expected_output);
}

// Тест на фул бэкап
TEST_F(BackupTests, FullBackup) {
  RunCommand("./bin/my_backup full " + work.string() + " " +
             backup.string());
  ASSERT_TRUE(file_sys::exists(backup / "last_full.txt"));

  std::ifstream last_full_backup((backup / "last_full.txt").string());
  std::ostringstream last_full;
  last_full << last_full_backup.rdbuf();
  file_sys::path dir_name = GetTimeName();

  EXPECT_TRUE(file_sys::exists(backup / dir_name / "file1.txt"));
  EXPECT_TRUE(file_sys::exists(backup / dir_name / "subdir1/file2.txt"));
  EXPECT_TRUE(
      file_sys::exists(backup / dir_name / "subdir1/subdir2/file3.txt"));

  EXPECT_EQ(dir_name.string(), last_full.str());
}

// Тест на инкрементный бэкап без предыдущего фул бэкапа
TEST_F(BackupTests, IncrementalBackupWithoutFull) {
  RunCommand("./bin/my_backup incremental " + work.string() + " " +
             backup.string());
  file_sys::path dir_name = GetTimeName();

  EXPECT_TRUE(file_sys::exists(backup / dir_name / "file1.txt"));
  EXPECT_TRUE(file_sys::exists(backup / dir_name / "subdir1/file2.txt"));
  EXPECT_TRUE(
      file_sys::exists(backup / dir_name / "subdir1/subdir2/file3.txt"));

  ASSERT_TRUE(file_sys::exists(backup / "last_full.txt"));
  EXPECT_EQ(ReadFile(backup / "last_full.txt"), dir_name.string());
}

// Тест для проверки инкрементного бэкапа при наличии фулл бэкапа
TEST_F(BackupTests, IncrementalBackupWithFull) {
  RunCommand("./bin/my_backup full " + work.string() + " " +
             backup.string());
  file_sys::path full_dir_name = GetTimeName();

  std::ofstream(work / "file1.txt", std::ios::out) << "Changed test file 1";
  std::ofstream(work / "subdir1/file2.txt", std::ios::out)
      << "Changed test file 2";
  std::ofstream(work / "subdir1/subdir2/file3.txt", std::ios::out)
      << "Changed test file 3";
  sleep(1);

  RunCommand("./bin/my_backup incremental " + work.string() + " " +
             backup.string());
  file_sys::path dir_name = GetTimeName();

  EXPECT_TRUE(file_sys::exists(backup / dir_name / "file1.txt"));
  EXPECT_TRUE(file_sys::exists(backup / dir_name / "subdir1/file2.txt"));
  EXPECT_TRUE(
      file_sys::exists(backup / dir_name / "subdir1/subdir2/file3.txt"));

  EXPECT_EQ(ReadFile(backup / dir_name / "file1.txt"), "Changed test file 1");
  EXPECT_EQ(ReadFile(backup / dir_name / "subdir1/file2.txt"),
            "Changed test file 2");
  EXPECT_EQ(ReadFile(backup / dir_name / "subdir1/subdir2/file3.txt"),
            "Changed test file 3");

  ASSERT_TRUE(file_sys::exists(backup / "last_full.txt"));
  EXPECT_EQ(ReadFile(backup / "last_full.txt"), full_dir_name.string());
}

// Тест на ошибку доступа к файлам
TEST_F(BackupTests, PermissionDeniedReadInWork) {
  std::ofstream test_file(work / "test_file.txt");
  test_file << "Hello, World!";
  test_file.close();
  file_sys::permissions(work / "test_file.txt", file_sys::perms::none);

  std::string output = RunCommand("./bin/my_backup full " +
                                  work.string() + " " + backup.string());
  std::string expected_out =
      "Упс, кажется, программа завершилась с ошибкой!\n\n"
      "Нет права на копирование файла в директорию, в которой вы хотите "
      "создать "
      "резервную копию\nПоменяйте права на файлы\n\nПопробуйте снова после "
      "исправления ошибки!\n";
  EXPECT_EQ(output, expected_out);
}

// Тест на создание резервной копии в недоступную директорию
TEST_F(BackupTests, PermissionDeniedWriteInBackup) {
  file_sys::permissions(backup, file_sys::perms::none);

  std::string output = RunCommand("./bin/my_backup full " +
                                  work.string() + " " + backup.string());
  std::string expected_out =
      "Упс, кажется, программа завершилась с ошибкой!\n\n"
      "Нет права чтобы создать папку внутри директории для бэкапа\nПоменяйте "
      "права на файлы внутри "
      " директории для копирования\n\nПопробуйте снова после "
      "исправления ошибки!\n";
  EXPECT_EQ(output, expected_out);
  file_sys::permissions(backup, file_sys::perms::all);
}

// Тест на проверку восстановления full backup
TEST_F(BackupTests, FullRestore) {
  RunCommand("./bin/my_backup full " + work.string() + " " +
             backup.string());
  file_sys::path full_dir_name = GetTimeName();

  std::ofstream(work / "file1.txt", std::ios::out) << "Changed test file 1";

  std::ofstream(work / "subdir/file2.txt", std::ios::out)
      << "Changed test file 2";
  file_sys::remove(work / "subdir1/subdir2/file3.txt");

  RunCommand("./bin/my_restore " + (backup / full_dir_name).string() + " " +
             work.string());

  EXPECT_TRUE(file_sys::exists(work / "file1.txt"));
  EXPECT_TRUE(file_sys::exists(work / "subdir1/file2.txt"));
  EXPECT_TRUE(file_sys::exists(work / "subdir1/subdir2/file3.txt"));

  EXPECT_EQ(ReadFile(work / "file1.txt"), "Test file 1");
  EXPECT_EQ(ReadFile(work / "subdir1/file2.txt"), "Test file 2");
}

// Тест на проверку восстановления increment backup
TEST_F(BackupTests, IncrementalRestore) {
  RunCommand("./bin/my_backup incremental " + work.string() + " " +
             backup.string());
  file_sys::path full_dir_name = GetTimeName();
  
  std::ofstream(work / "file1.txt", std::ios::out) << "Changed test file 1";
  file_sys::remove_all(work / "subdir1/subdir2");
  RunCommand("./bin/my_restore " + (backup / full_dir_name).string() + " " +
             work.string());
  EXPECT_TRUE(file_sys::exists(work / "file1.txt"));
  EXPECT_TRUE(file_sys::exists(work / "subdir1/file2.txt"));
  EXPECT_TRUE(file_sys::exists(work / "subdir1/subdir2"));

  std::string output = ReadFile(work / "file1.txt");
  EXPECT_EQ(output, "Test file 1");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  std::string command_backup =
      "g++ ./my_backup/my_backup.cpp -o ./bin/my_backup";
  std::string command_restore =
      "g++ ./my_restore/my_restore.cpp -o ./bin/my_restore";
  std::system(command_backup.c_str());
  std::system(command_restore.c_str());
  return RUN_ALL_TESTS();
}