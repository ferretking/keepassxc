/*
 *  Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PasswordEdit.h"

#include "core/Config.h"
#include "core/FilePath.h"
#include "gui/Font.h"
#include "gui/PasswordGeneratorWidget.h"

#include <QDialog>
#include <QVBoxLayout>

namespace
{
    const QColor CorrectSoFarColor(255, 205, 15);
    const QColor ErrorColor(255, 125, 125);
} // namespace

PasswordEdit::PasswordEdit(QWidget* parent)
    : QLineEdit(parent)
{
    const QIcon errorIcon = filePath()->icon("status", "dialog-error");
    m_errorAction = addAction(errorIcon, QLineEdit::TrailingPosition);
    m_errorAction->setVisible(false);
    m_errorAction->setToolTip(tr("Passwords do not match"));

    const QIcon correctIcon = filePath()->icon("actions", "dialog-ok");
    m_correctAction = addAction(correctIcon, QLineEdit::TrailingPosition);
    m_correctAction->setVisible(false);
    m_correctAction->setToolTip(tr("Passwords match so far"));

    setEchoMode(QLineEdit::Password);

    // use a monospace font for the password field
    QFont passwordFont = Font::fixedFont();
    passwordFont.setLetterSpacing(QFont::PercentageSpacing, 110);
    setFont(passwordFont);

    m_toggleVisibleAction = new QAction(
        filePath()->icon("actions", "password-show-off"),
        tr("Toggle Password (%1)").arg(QKeySequence(Qt::CTRL + Qt::Key_H).toString(QKeySequence::NativeText)));
    m_toggleVisibleAction->setCheckable(true);
    m_toggleVisibleAction->setShortcut(Qt::CTRL + Qt::Key_H);
    m_toggleVisibleAction->setShortcutContext(Qt::WidgetShortcut);
    addAction(m_toggleVisibleAction, QLineEdit::TrailingPosition);
    connect(m_toggleVisibleAction, &QAction::triggered, this, &PasswordEdit::setShowPassword);

    m_passwordGeneratorAction = new QAction(
        filePath()->icon("actions", "password-generator"),
        tr("Generate Password (%1)").arg(QKeySequence(Qt::CTRL + Qt::Key_G).toString(QKeySequence::NativeText)));
    m_passwordGeneratorAction->setShortcut(Qt::CTRL + Qt::Key_G);
    m_passwordGeneratorAction->setShortcutContext(Qt::WidgetShortcut);
    addAction(m_passwordGeneratorAction, QLineEdit::TrailingPosition);
    m_passwordGeneratorAction->setVisible(false);
}

void PasswordEdit::setRepeatPartner(PasswordEdit* repeatEdit)
{
    m_repeatPasswordEdit = repeatEdit;
    m_repeatPasswordEdit->setParentPasswordEdit(this);

    connect(this, SIGNAL(textChanged(QString)), m_repeatPasswordEdit, SLOT(autocompletePassword(QString)));
    connect(this, SIGNAL(textChanged(QString)), m_repeatPasswordEdit, SLOT(updateRepeatStatus()));
    connect(m_repeatPasswordEdit, SIGNAL(textChanged(QString)), m_repeatPasswordEdit, SLOT(updateRepeatStatus()));
}

void PasswordEdit::setParentPasswordEdit(PasswordEdit* parent)
{
    m_parentPasswordEdit = parent;
    // Hide actions
    m_toggleVisibleAction->setVisible(false);
    m_passwordGeneratorAction->setVisible(false);
}

void PasswordEdit::enablePasswordGenerator(bool signalOnly)
{
    disconnect(m_passwordGeneratorAction);
    m_passwordGeneratorAction->setVisible(true);

    if (signalOnly) {
        connect(m_passwordGeneratorAction, &QAction::triggered, this, &PasswordEdit::togglePasswordGenerator);
    } else {
        connect(m_passwordGeneratorAction, &QAction::triggered, this, &PasswordEdit::popupPasswordGenerator);
    }
}

void PasswordEdit::setShowPassword(bool show)
{
    setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    m_toggleVisibleAction->setIcon(filePath()->icon("actions", show ? "password-show-on" : "password-show-off"));
    m_toggleVisibleAction->setChecked(show);

    if (m_repeatPasswordEdit) {
        m_repeatPasswordEdit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        if (config()->get("security/passwordsrepeat").toBool()) {
            m_repeatPasswordEdit->setEnabled(!show);
            m_repeatPasswordEdit->setText(text());
        } else {
            m_repeatPasswordEdit->setEnabled(true);
        }
    }
}

bool PasswordEdit::isPasswordVisible() const
{
    return echoMode() == QLineEdit::Normal;
}

void PasswordEdit::popupPasswordGenerator()
{
    QDialog pwDialog;
    pwDialog.setWindowTitle(tr("Generate master password"));

    auto pwGenerator = new PasswordGeneratorWidget(&pwDialog);
    auto layout = new QVBoxLayout();
    pwDialog.setLayout(layout);
    layout->addWidget(pwGenerator);

    pwGenerator->setStandaloneMode(false);
    pwGenerator->setPasswordVisible(isPasswordVisible());

    connect(pwGenerator, SIGNAL(closePasswordGenerator()), &pwDialog, SLOT(close()));
    connect(pwGenerator, SIGNAL(appliedPassword(QString)), SLOT(setText(QString)));
    if (m_repeatPasswordEdit) {
        connect(pwGenerator, SIGNAL(appliedPassword(QString)), m_repeatPasswordEdit, SLOT(setText(QString)));
    }

    pwDialog.exec();
}

void PasswordEdit::updateRepeatStatus()
{
    static const auto stylesheetTemplate = QStringLiteral("QLineEdit { background: %1; }");
    if (!m_parentPasswordEdit) {
        return;
    }

    const auto& otherPassword = m_parentPasswordEdit->text();
    const auto& password = text();
    if (otherPassword != password) {
        bool isCorrect = true;
        if (!password.isEmpty() && otherPassword.startsWith(password)) {
            setStyleSheet(stylesheetTemplate.arg(CorrectSoFarColor.name()));
        } else {
            setStyleSheet(stylesheetTemplate.arg(ErrorColor.name()));
            isCorrect = false;
        }
        m_correctAction->setVisible(isCorrect);
        m_errorAction->setVisible(!isCorrect);
    } else {
        m_correctAction->setVisible(false);
        m_errorAction->setVisible(false);
        setStyleSheet("");
    }
}

void PasswordEdit::autocompletePassword(const QString& password)
{
    if (config()->get("security/passwordsrepeat").toBool() && echoMode() == QLineEdit::Normal) {
        setText(password);
    }
}
