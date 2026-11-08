#pragma once

#include <array>
#include <string_view>

#include "product/RapidReader.h"

namespace product {
    enum class Text {
        Starting, LibrarySetup, DisplaySetup, ReaderSetup, Ready,
        UsbCompanion, Connected, Receiving, Sending, Processing,
        BrowserHint, KeepConnected, ProcessingHint, UsbStorage, EjectHint,
        StoragePreparing, StorageReadWrite, StorageReadOnly, StorageEjected,
        StorageError, UsbError, UsbDisabled, Idle,
        Tutorial, TutorialPrevious, TutorialNext, TutorialSkip, TutorialFinish,
        TutorialBooks, TutorialBooksLine1, TutorialBooksLine2,
        TutorialControls, TutorialControlsLine1, TutorialControlsLine2,
        TutorialGestures, TutorialGesturesLine1, TutorialGesturesLine2,
        TutorialDisplay, TutorialDisplayLine1, TutorialDisplayLine2,
        Count,
    };

    // Product-only messages do not change the upstream locale-pack ABI.
    constexpr std::string_view text(Text key, std::string_view locale = kDefaultLocale) {
        constexpr std::array<std::array<std::string_view, 2>, static_cast<size_t>(Text::Count)> messages{{
            {"Запуск системы", "Starting system"},
            {"Подготовка библиотеки", "Preparing library"},
            {"Настройка экрана", "Preparing display"},
            {"Подготовка чтения", "Preparing reader"},
            {"Готово", "Ready"},
            {"USB / Библиотека", "USB / Library"},
            {"Подключено", "Connected"},
            {"Приём данных", "Receiving data"},
            {"Отправка данных", "Sending data"},
            {"Обработка", "Processing"},
            {"Не закрывайте вкладку браузера", "Keep the browser tab open"},
            {"Не отключайте USB-кабель", "Keep the USB cable connected"},
            {"Применяем изменения", "Applying changes"},
            {"USB / Карта памяти", "USB / Memory card"},
            {"Перед отключением извлеките диск на компьютере", "Eject the drive on your computer before disconnecting"},
            {"Подготовка карты", "Preparing memory card"},
            {"Чтение и запись", "Read and write"},
            {"Только чтение", "Read only"},
            {"Диск извлечён", "Drive ejected"},
            {"Ошибка карты памяти", "Memory card error"},
            {"Ошибка подключения USB", "USB connection error"},
            {"Передача по USB недоступна", "USB transfer unavailable"},
            {"Ожидание", "Idle"},
            {"Знакомство", "Quick start"},
            {"Назад", "Back"},
            {"Далее", "Next"},
            {"Пропустить", "Skip"},
            {"Начать", "Start reading"},
            {"Книги на устройстве", "Your books"},
            {"Загрузите книгу по USB через конвертер RapidReader.", "Send a book over USB using the RapidReader converter."},
            {"Книги — на microSD (FAT32). Откройте «Библиотека».", "Books need a microSD card (FAT32). Open Library to choose one."},
            {"Чтение и меню", "Reading and menus"},
            {"BOOT — старт / пауза. PWR — открыть меню.", "BOOT starts / pauses reading. PWR opens the menu."},
            {"В меню: продолжить, библиотека и главы.", "The menu has Resume, Library and Chapters."},
            {"Скорость и навигация", "Speed and navigation"},
            {"На паузе: свайп вверх / вниз меняет скорость.", "While paused: swipe up / down to change reading speed."},
            {"Свайп влево / вправо — перемещение по тексту.", "Swipe left / right while paused to move through the text."},
            {"Экран под вас", "Make it yours"},
            {"В «Настройках»: яркость, тема, язык и режим ожидания.", "Settings has brightness, theme, language and standby controls."},
            {"Прогресс чтения и скорость сохраняются автоматически.", "Your reading position and speed are saved automatically."},
        }};
        const auto index = static_cast<size_t>(key);
        return index < messages.size() ? messages[index][locale == "en" ? 1 : 0] : std::string_view{};
    }
} // namespace product
