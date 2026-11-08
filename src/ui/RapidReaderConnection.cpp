#include "ui/RapidReaderConnection.h"

#include "product/RapidReaderText.h"
#include "ui/RapidReaderBrand.h"
#include "ui/RapidReaderUi.h"

namespace screens {
    namespace {
        using product::Text;

        void connection(ui::Context& ui, std::string_view caption, std::string_view title,
                        std::string_view hint, int progress) {
            // A separate frame key cleanly transitions from boot, jobs and menus.
            ui.beginFrame(0xfd);
            ui.label({24, 0, 400, 40}, "RAPIDREADER", 1, ui::themes::Foreground);
            ui.label({540, 0, 76, 40}, "USB", 1, ui::themes::Accent, ui::TextAlign::Right);
            const ui::Rect rule{24, 43, static_cast<int16_t>(ui.width() - 48), 1};
            if (ui.redraw(rule, 1))
                ui.paint(rule, [&](Arduino_GFX& output, ui::Rect r) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(ui::themes::Outline));
                });
            rapidReaderLogo(ui, {24, 57, 48, 48});
            ui.label({92, 49, 524, 22}, rapid::uppercase(caption), 1, ui::themes::Muted);
            ui.label({92, 74, 524, 38}, rapid::uppercase(title), 2, ui::themes::Foreground);
            ui.label({24, 120, 500, 24}, hint, 1,
                     ui::themes::Muted);
            ui.label({540, 120, 76, 24}, progress >= 0 ? std::to_string(progress) + "%" : std::string{}, 1,
                     ui::themes::Foreground, ui::TextAlign::Right);
            const ui::Rect track{24, 154, static_cast<int16_t>(ui.width() - 48), 2};
            if (ui.redraw(track, progress + 2))
                ui.paint(track, [&](Arduino_GFX& output, ui::Rect r) {
                    output.fillRect(r.x, r.y, r.w, r.h, ui.color(ui::themes::ProgressTrack));
                    // Idle shows a brand accent; determinate progress only uses real byte counts.
                    const int16_t width = progress < 0 ? 48 : static_cast<int16_t>(r.w * progress / 100);
                    output.fillRect(r.x, r.y, width, r.h, ui.color(ui::themes::Accent));
                });
            ui.endFrame();
        }
    } // namespace

    void rapidReaderUsb(ui::Context& ui, const companion::serial::TransferStatus& transfer) {
        using companion::serial::TransferActivity;
        Text title = Text::Connected;
        Text hint = Text::BrowserHint;
        switch (transfer.activity) {
        case TransferActivity::Receiving: title = Text::Receiving; hint = Text::KeepConnected; break;
        case TransferActivity::Sending: title = Text::Sending; hint = Text::KeepConnected; break;
        case TransferActivity::Processing: title = Text::Processing; hint = Text::ProcessingHint; break;
        default: break;
        }
        connection(ui, product::text(Text::UsbCompanion, ui.locale()), product::text(title, ui.locale()),
                   transfer.name.empty() ? product::text(hint, ui.locale()) : transfer.name,
                   transfer.percentage());
    }

    void rapidReaderUsbStorage(ui::Context& ui, std::string_view status) {
        const auto title = status == "Preparing SD" ? Text::StoragePreparing
                         : status == "Mounted read/write" ? Text::StorageReadWrite
                         : status == "Mounted read-only" ? Text::StorageReadOnly
                         : status == "Ejected" ? Text::StorageEjected
                         : status == "SD init failed" ? Text::StorageError
                         : status == "USB MSC failed" ? Text::UsbError
                         : status == "USB transfer disabled" ? Text::UsbDisabled : Text::Idle;
        connection(ui, product::text(Text::UsbStorage, ui.locale()), product::text(title, ui.locale()),
                   product::text(title == Text::StorageEjected ? Text::Ready : Text::EjectHint, ui.locale()), -1);
    }
} // namespace screens
