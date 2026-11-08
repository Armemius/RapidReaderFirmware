#pragma once

#include "companion/serial/CompanionTransferStatus.h"
#include "ui/Ui.h"

namespace screens {
    void rapidReaderUsb(ui::Context& ui, const companion::serial::TransferStatus& transfer = {});
    void rapidReaderUsbStorage(ui::Context& ui, std::string_view status);
} // namespace screens
