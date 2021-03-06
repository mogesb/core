/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* NumbersImportFilter: Sets up the filter, and calls DocumentCollector
 * to do the actual filtering
 *
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <libetonyek/libetonyek.h>

#include <cppuhelper/supportsservice.hxx>

#include "NumbersImportFilter.hxx"

using com::sun::star::uno::Sequence;
using com::sun::star::uno::XInterface;
using com::sun::star::uno::RuntimeException;
using com::sun::star::uno::XComponentContext;

using libetonyek::EtonyekDocument;

bool NumbersImportFilter::doImportDocument(librevenge::RVNGInputStream &rInput, OdsGenerator &rGenerator, utl::MediaDescriptor &)
{
    return EtonyekDocument::parse(&rInput, &rGenerator);
}

bool NumbersImportFilter::doDetectFormat(librevenge::RVNGInputStream &rInput, OUString &rTypeName)
{
    EtonyekDocument::Type type = EtonyekDocument::TYPE_UNKNOWN;
    const EtonyekDocument::Confidence confidence = EtonyekDocument::isSupported(&rInput, &type);
    if ((confidence == EtonyekDocument::CONFIDENCE_EXCELLENT) && (type == EtonyekDocument::TYPE_NUMBERS))
    {
        rTypeName = "calc_AppleNumbers";
        return true;
    }

    return false;
}

void NumbersImportFilter::doRegisterHandlers(OdsGenerator &)
{
}

// XServiceInfo
OUString SAL_CALL NumbersImportFilter::getImplementationName()
throw (RuntimeException, std::exception)
{
    return OUString("org.libreoffice.comp.Calc.NumbersImportFilter");
}

sal_Bool SAL_CALL NumbersImportFilter::supportsService(const OUString &rServiceName)
throw (RuntimeException, std::exception)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence< OUString > SAL_CALL NumbersImportFilter::getSupportedServiceNames()
throw (RuntimeException, std::exception)
{
    Sequence < OUString > aRet(2);
    OUString *pArray = aRet.getArray();
    pArray[0] =  "com.sun.star.document.ImportFilter";
    pArray[1] =  "com.sun.star.document.ExtendedTypeDetection";
    return aRet;
}

extern "C"
SAL_DLLPUBLIC_EXPORT css::uno::XInterface *SAL_CALL
org_libreoffice_comp_Calc_NumbersImportFilter_get_implementation(
    css::uno::XComponentContext *const context,
    const css::uno::Sequence<css::uno::Any> &)
{
    return cppu::acquire(new NumbersImportFilter(context));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
