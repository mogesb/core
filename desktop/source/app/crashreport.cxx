/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <desktop/crashreport.hxx>
#include <rtl/bootstrap.hxx>
#include <osl/file.hxx>

#include <config_version.h>
#include <config_folders.h>

#include <string>
#include <fstream>

osl::Mutex CrashReporter::maMutex;

#if HAVE_FEATURE_BREAKPAD

void CrashReporter::AddKeyValue(const OUString& rKey, const OUString& rValue)
{
    osl::MutexGuard aGuard(maMutex);
    std::string ini_path = getIniFileName();
    std::ofstream ini_file(ini_path, std::ios_base::app);
    ini_file << rtl::OUStringToOString(rKey, RTL_TEXTENCODING_UTF8).getStr() << "=";
    ini_file << rtl::OUStringToOString(rValue, RTL_TEXTENCODING_UTF8).getStr() << "\n";
}

#endif

void CrashReporter::writeCommonInfo()
{
    // limit the amount of code that needs to be executed before the crash reporting
    std::string ini_path = CrashReporter::getIniFileName();
    std::ofstream minidump_file(ini_path, std::ios_base::trunc);
    minidump_file << "ProductName=LibreOffice\n";
    minidump_file << "Version=" LIBO_VERSION_DOTTED "\n";
    minidump_file << "URL=http://127.0.0.1:8000/submit\n";
    minidump_file.close();
}

std::string CrashReporter::getIniFileName()
{
    OUString url("${$BRAND_BASE_DIR/" LIBO_ETC_FOLDER "/" SAL_CONFIGFILE("bootstrap") ":UserInstallation}/crash/");
    rtl::Bootstrap::expandMacros(url);
    osl::Directory::create(url);

    OString aUrl = OUStringToOString(url, RTL_TEXTENCODING_UTF8);
    std::string aRet(aUrl.getStr());
    return aRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
