/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <osl/file.h>
#include <tools/stream.hxx>
#include <tools/vcompat.hxx>
#include <tools/debug.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <unotools/tempfile.hxx>
#include <ucbhelper/content.hxx>
#include <vcl/graph.hxx>
#include <vcl/gfxlink.hxx>
#include <vcl/cvtgrf.hxx>
#include <com/sun/star/ucb/CommandAbortedException.hpp>
#include <memory>

GfxLink::GfxLink() :
    myType      ( GfxLinkType::None ),
    mpBuf       ( nullptr ),
    mpSwap      ( nullptr ),
    mnBufSize   ( 0 ),
    mnUserId    ( 0UL ),
    mpImpData   ( new ImpGfxLink )
{
}

GfxLink::GfxLink( const GfxLink& rGfxLink ) :
    mpImpData( new ImpGfxLink )
{
    ImplCopy( rGfxLink );
}

GfxLink::GfxLink( sal_uInt8* pBuf, sal_uInt32 nSize, GfxLinkType nType ) :
    mpImpData( new ImpGfxLink )
{
    DBG_ASSERT( pBuf != nullptr && nSize,
                "GfxLink::GfxLink(): empty/NULL buffer given" );

    myType = nType;
    mnBufSize = nSize;
    mpSwap = nullptr;
    mnUserId = 0UL;
    mpBuf = new ImpBuffer( pBuf );
}

GfxLink::~GfxLink()
{
    if( mpBuf && !( --mpBuf->mnRefCount ) )
        delete mpBuf;

    if( mpSwap && !( --mpSwap->mnRefCount ) )
        delete mpSwap;

    delete mpImpData;
}

GfxLink& GfxLink::operator=( const GfxLink& rGfxLink )
{
    if( &rGfxLink != this )
    {
        if ( mpBuf && !( --mpBuf->mnRefCount ) )
            delete mpBuf;

        if( mpSwap && !( --mpSwap->mnRefCount ) )
            delete mpSwap;

        ImplCopy( rGfxLink );
    }

    return *this;
}

bool GfxLink::IsEqual( const GfxLink& rGfxLink ) const
{
    bool bIsEqual = false;

    if ( ( mnBufSize == rGfxLink.mnBufSize ) && ( myType == rGfxLink.myType ) )
    {
        const sal_uInt8* pSource = GetData();
        const sal_uInt8* pDest = rGfxLink.GetData();
        sal_uInt32 nSourceSize = GetDataSize();
        sal_uInt32 nDestSize = rGfxLink.GetDataSize();
        if ( pSource && pDest && ( nSourceSize == nDestSize ) )
        {
            bIsEqual = memcmp( pSource, pDest, nSourceSize ) == 0;
        }
        else if ( ( pSource == nullptr ) && ( pDest == nullptr ) )
            bIsEqual = true;
    }
    return bIsEqual;
}

void GfxLink::ImplCopy( const GfxLink& rGfxLink )
{
    mnBufSize = rGfxLink.mnBufSize;
    myType = rGfxLink.myType;
    mpBuf = rGfxLink.mpBuf;
    mpSwap = rGfxLink.mpSwap;
    mnUserId = rGfxLink.mnUserId;
    *mpImpData = *rGfxLink.mpImpData;

    if( mpBuf )
        mpBuf->mnRefCount++;

    if( mpSwap )
        mpSwap->mnRefCount++;
}


bool GfxLink::IsNative() const
{
    return( myType >= GFX_LINK_FIRST_NATIVE_ID && myType <= GFX_LINK_LAST_NATIVE_ID );
}


const sal_uInt8* GfxLink::GetData() const
{
    if( IsSwappedOut() )
        const_cast<GfxLink*>(this)->SwapIn();

    return( mpBuf ? mpBuf->mpBuffer : nullptr );
}


void GfxLink::SetPrefSize( const Size& rPrefSize )
{
    mpImpData->maPrefSize = rPrefSize;
    mpImpData->mbPrefSizeValid = true;
}


void GfxLink::SetPrefMapMode( const MapMode& rPrefMapMode )
{
    mpImpData->maPrefMapMode = rPrefMapMode;
    mpImpData->mbPrefMapModeValid = true;
}


bool GfxLink::LoadNative( Graphic& rGraphic )
{
    bool bRet = false;

    if( IsNative() && mnBufSize )
    {
        const sal_uInt8* pData = GetData();

        if( pData )
        {
            SvMemoryStream    aMemStm;
            ConvertDataFormat nCvtType;

            aMemStm.SetBuffer( const_cast<sal_uInt8*>(pData), mnBufSize, mnBufSize );

            switch( myType )
            {
                case GfxLinkType::Native_GIF: nCvtType  = ConvertDataFormat::GIF; break;
                case GfxLinkType::Native_BMP: nCvtType  = ConvertDataFormat::BMP; break;
                case GfxLinkType::Native_JPEG: nCvtType = ConvertDataFormat::JPEG; break;
                case GfxLinkType::Native_PNG: nCvtType  = ConvertDataFormat::PNG; break;
                case GfxLinkType::Native_TIFF: nCvtType = ConvertDataFormat::TIFF; break;
                case GfxLinkType::Native_WMF: nCvtType  = ConvertDataFormat::WMF; break;
                case GfxLinkType::Native_MET: nCvtType  = ConvertDataFormat::MET; break;
                case GfxLinkType::Native_PICT: nCvtType = ConvertDataFormat::PICT; break;
                case GfxLinkType::Native_SVG: nCvtType  = ConvertDataFormat::SVG; break;

                default: nCvtType = ConvertDataFormat::Unknown; break;
            }

            if( nCvtType != ConvertDataFormat::Unknown && ( GraphicConverter::Import( aMemStm, rGraphic, nCvtType ) == ERRCODE_NONE ) )
                bRet = true;
        }
    }

    return bRet;
}

void GfxLink::SwapOut()
{
    if( !IsSwappedOut() && mpBuf )
    {
        mpSwap = new ImpSwap( mpBuf->mpBuffer, mnBufSize );

        if( !mpSwap->IsSwapped() )
        {
            delete mpSwap;
            mpSwap = nullptr;
        }
        else
        {
            if( !( --mpBuf->mnRefCount ) )
                delete mpBuf;

            mpBuf = nullptr;
        }
    }
}

void GfxLink::SwapIn()
{
    if( IsSwappedOut() )
    {
        mpBuf = new ImpBuffer( mpSwap->GetData() );

        if( !( --mpSwap->mnRefCount ) )
            delete mpSwap;

        mpSwap = nullptr;
    }
}

bool GfxLink::ExportNative( SvStream& rOStream ) const
{
    if( GetDataSize() )
    {
        if( IsSwappedOut() )
            mpSwap->WriteTo( rOStream );
        else if( GetData() )
            rOStream.Write( GetData(), GetDataSize() );
    }

    return ( rOStream.GetError() == ERRCODE_NONE );
}

SvStream& WriteGfxLink( SvStream& rOStream, const GfxLink& rGfxLink )
{
    std::unique_ptr<VersionCompat> pCompat(new VersionCompat( rOStream, StreamMode::WRITE, 2 ));

    // Version 1
    rOStream.WriteUInt16( static_cast< sal_uInt16 >( rGfxLink.GetType() )).WriteUInt32( rGfxLink.GetDataSize() ).WriteUInt32( rGfxLink.GetUserId() );

    // Version 2
    WritePair( rOStream, rGfxLink.GetPrefSize() );
    WriteMapMode( rOStream, rGfxLink.GetPrefMapMode() );

    pCompat.reset(); // destructor writes stuff into the header

    if( rGfxLink.GetDataSize() )
    {
        if( rGfxLink.IsSwappedOut() )
            rGfxLink.mpSwap->WriteTo( rOStream );
        else if( rGfxLink.GetData() )
            rOStream.Write( rGfxLink.GetData(), rGfxLink.GetDataSize() );
    }

    return rOStream;
}

SvStream& ReadGfxLink( SvStream& rIStream, GfxLink& rGfxLink)
{
    Size            aSize;
    MapMode         aMapMode;
    sal_uInt8 *     pBuf;
    bool            bMapAndSizeValid( false );
    std::unique_ptr<VersionCompat>  pCompat(new VersionCompat( rIStream, StreamMode::READ ));

    // Version 1
    sal_uInt16 theType;
    sal_uInt32 nSize;
    sal_uInt32 nUserId;
    rIStream.ReadUInt16( theType ).ReadUInt32( nSize ).ReadUInt32( nUserId );

    if( pCompat->GetVersion() >= 2 )
    {
        ReadPair( rIStream, aSize );
        ReadMapMode( rIStream, aMapMode );
        bMapAndSizeValid = true;
    }

    pCompat.reset(); // destructor writes stuff into the header

    pBuf = new sal_uInt8[ nSize ];
    rIStream.Read( pBuf, nSize );

    rGfxLink = GfxLink( pBuf, nSize, static_cast< GfxLinkType >( theType ) );
    rGfxLink.SetUserId( nUserId );

    if( bMapAndSizeValid )
    {
        rGfxLink.SetPrefSize( aSize );
        rGfxLink.SetPrefMapMode( aMapMode );
    }

    return rIStream;
}

ImpSwap::ImpSwap( sal_uInt8* pData, sal_uLong nDataSize ) :
            mnDataSize( nDataSize ),
            mnRefCount( 1UL )
{
    if( pData && mnDataSize )
    {
        ::utl::TempFile aTempFile;

        maURL = aTempFile.GetURL();
        if( !maURL.isEmpty() )
        {
            std::unique_ptr<SvStream> xOStm(::utl::UcbStreamHelper::CreateStream( maURL, STREAM_READWRITE | StreamMode::SHARE_DENYWRITE ));
            if( xOStm )
            {
                xOStm->Write( pData, mnDataSize );
                bool bError = ( ERRCODE_NONE != xOStm->GetError() );
                xOStm.reset();

                if( bError )
                {
                    osl_removeFile( maURL.pData );
                    maURL.clear();
                }
            }
        }
    }
}

ImpSwap::~ImpSwap()
{
    if( IsSwapped() )
        osl_removeFile( maURL.pData );
}

sal_uInt8* ImpSwap::GetData() const
{
    sal_uInt8* pData;

    if( IsSwapped() )
    {
        std::unique_ptr<SvStream> xIStm(::utl::UcbStreamHelper::CreateStream( maURL, STREAM_READWRITE ));
        if( xIStm )
        {
            pData = new sal_uInt8[ mnDataSize ];
            xIStm->Read( pData, mnDataSize );
            bool bError = ( ERRCODE_NONE != xIStm->GetError() );
            sal_Size nActReadSize = xIStm->Tell();
            if (nActReadSize != mnDataSize)
            {
                bError = true;
            }
            xIStm.reset();

            if( bError )
            {
                delete[] pData;
                pData = nullptr;
            }
        }
        else
            pData = nullptr;
    }
    else
        pData = nullptr;

    return pData;
}

void ImpSwap::WriteTo( SvStream& rOStm ) const
{
    sal_uInt8* pData = GetData();

    if( pData )
    {
        rOStm.Write( pData, mnDataSize );
        delete[] pData;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
