/*
    Copyright 2016-2023 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include "AboutDialog.h"

#include <QDesktopServices>
#include <QUrl>

#include "ui_AboutDialog.h"

#include "version.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Community release version, with the upstream core version in small print
    ui->lblVersionInfo->setTextFormat(Qt::RichText);
    ui->lblVersionInfo->setText(
        tr("Version %1").arg(QString(MELONDS_COMMUNITY_VERSION)).toHtmlEscaped()
        + "<br><span style=\"color: gray; font-size: small;\">"
        + tr("Based on melonDS %1").arg(QString(MELONDS_VERSION)).toHtmlEscaped()
        + "</span>");
#ifdef MELONDS_EMBED_BUILD_INFO
    ui->lblBuildInfo->setText(
        tr("Branch: %1\nCommit: %2\nBuilt by: %3")
            .arg(QString(MELONDS_GIT_BRANCH), QString(MELONDS_GIT_HASH), QString(MELONDS_BUILD_PROVIDER))
    );
#else
    ui->lblBuildInfo->hide();
#endif
    adjustSize();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::openWebsite()
{
    QDesktopServices::openUrl(QUrl(MELONDS_URL));
}

void AboutDialog::openGitHub()
{
    QDesktopServices::openUrl(QUrl(MELONDS_COMMUNITY_URL));
}
