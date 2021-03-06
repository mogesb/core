/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*************************************************************************
 *
 *  Effective License of whole file:
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License version 2.1, as published by the Free Software Foundation.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *    MA  02111-1307  USA
 *
 *  Parts "Copyright by Sun Microsystems, Inc" prior to August 2011:
 *
 *    The Contents of this file are made available subject to the terms of
 *    the GNU Lesser General Public License Version 2.1
 *
 *    Copyright: 2000 by Sun Microsystems, Inc.
 *
 *    Contributor(s): Joerg Budischewski
 *
 *  All parts contributed on or after August 2011:
 *
 *    This Source Code Form is subject to the terms of the Mozilla Public
 *    License, v. 2.0. If a copy of the MPL was not distributed with this
 *    file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 ************************************************************************/

#include <rtl/ustrbuf.hxx>
#include <rtl/strbuf.hxx>

#include <com/sun/star/sdbc/XRow.hpp>
#include <com/sun/star/sdbc/DataType.hpp>
#include <com/sun/star/sdbc/ColumnValue.hpp>

#include <cppuhelper/implbase.hxx>

#include "pq_xcolumns.hxx"
#include "pq_xcolumn.hxx"
#include "pq_statics.hxx"
#include "pq_tools.hxx"

using osl::MutexGuard;


using com::sun::star::beans::XPropertySet;
using com::sun::star::beans::XPropertyChangeListener;
using com::sun::star::beans::PropertyChangeEvent;

using com::sun::star::uno::Any;
using com::sun::star::uno::makeAny;
using com::sun::star::uno::UNO_QUERY;
using com::sun::star::uno::Reference;
using com::sun::star::uno::RuntimeException;

using com::sun::star::container::NoSuchElementException;

using com::sun::star::sdbc::XRow;
using com::sun::star::sdbc::XStatement;
using com::sun::star::sdbc::XResultSet;
using com::sun::star::sdbc::XDatabaseMetaData;
using com::sun::star::sdbc::SQLException;

namespace pq_sdbc_driver
{

static Any isCurrency( const OUString & typeName )
{
    sal_Bool b = typeName.equalsIgnoreAsciiCase("money");
    return Any( &b, cppu::UnoType<bool>::get() );
}

// static sal_Bool isAutoIncrement8( const OUString & typeName )
// {
//     return typeName.equalsIgnoreAsciiCase("serial8") ||
//         typeName.equalsIgnoreAsciiCase("bigserial");
// }

static Any isAutoIncrement( const OUString & defaultValue )
{
    sal_Bool ret = defaultValue.startsWith( "nextval(" );
//     printf( "%s %d\n",
//             OUStringToOString(defaultValue, RTL_TEXTENCODING_ASCII_US).getStr(),
//             ret );
//     {
//     static const char  * const serials[] =
//         {
//             "serial", "serial4", "serial8", "bigserial", 0
//         };
// s    sal_Bool b = sal_False;
//     for( int i = 0; !b && serials[i] ; i ++ )
//     {
//         b = b || typeName.equalsIgnoreAsciiCaseAscii( serials[i] );
//     }
    return Any ( &ret, cppu::UnoType<bool>::get() );
}

Columns::Columns(
        const ::rtl::Reference< RefCountedMutex > & refMutex,
        const ::com::sun::star::uno::Reference< com::sun::star::sdbc::XConnection >  & origin,
        ConnectionSettings *pSettings,
        const OUString &schemaName,
        const OUString &tableName)
    : Container( refMutex, origin, pSettings,  "COLUMN" ),
      m_schemaName( schemaName ),
      m_tableName( tableName )
{}

Columns::~Columns()
{}

OUString columnMetaData2SDBCX(
    ReflectionBase *pBase, const com::sun::star::uno::Reference< com::sun::star::sdbc::XRow > &xRow )
{
    Statics & st = getStatics();

    //  1. TABLE_CAT string => table catalog (may be NULL)
    //               => not supported
    //  2. TABLE_SCHEM string => table schema (may be NULL)
    //               => pg_namespace.nspname
    //  3. TABLE_NAME string => table name
    //               => pg_class.relname
    //  4. COLUMN_NAME string => column name
    //               => pg_attribure.attname
    //  5. DATA_TYPE short => SQL type from java.sql.Types
    //               => pg_type.typname => sdbc.DataType
    //  6. TYPE_NAME string => Data source dependent type name, for a UDT the
    //                         type name is fully qualified
    //               => pg_type.typname
    //  7. COLUMN_SIZE long => column size. For char or date types this is
    //                         the maximum number of characters, for numeric
    //                         or decimal types this is precision.
    //               => pg_type.typlen ( TODO: What is about variable size ? )
    //  8. BUFFER_LENGTH is not used.
    //               => not used
    //  9. DECIMAL_DIGITS long => the number of fractional digits
    //               => don't know ! TODO !
    //  10. NUM_PREC_RADIX long => Radix (typically either 10 or 2)
    //               => TODO ??
    //  11. NULLABLE long => is NULL allowed?
    //                      NO_NULLS - might not allow NULL values
    //                      NULABLE - definitely allows NULL values
    //                      NULLABLE_UNKNOWN - nullability unknown
    //               => pg_attribute.attnotnull
    //  12. REMARKS string => comment describing column (may be NULL )
    //               => Don't know, there does not seem to exist something like
    //                  that in postgres
    //  13. COLUMN_DEF string => default value (may be NULL)
    //               => pg_type.typdefault
    //  14. SQL_DATA_TYPE long => unused
    //               => empty
    //  15. SQL_DATETIME_SUB long => unused
    //               => empty
    //  16. CHAR_OCTET_LENGTH long => for char types the maximum number of
    //                                bytes in the column
    //               => pg_type.typlen
    //  17. ORDINAL_POSITION int => index of column in table (starting at 1)
    //                              pg_attribute.attnum
    //  18. IS_NULLABLE string => "NO" means column definitely does not allow
    //                            NULL values; "YES" means the column might
    //                            allow NULL values. An empty string means
    //                            nobody knows.
    //               => pg_attribute.attnotnull

    static const int COLUMN_NAME = 4;
    static const int DATA_TYPE = 5;
    static const int TYPE_NAME = 6;
    static const int COLUMN_SIZE = 7;
    static const int DECIMAL_DIGITS = 9;
    static const int IS_NULLABLE = 11;
    static const int DESCRIPTION = 12;
    static const int DEFAULT_VALUE = 13;

    OUString name = xRow->getString( COLUMN_NAME );
    OUString typeName  = xRow->getString( TYPE_NAME );

    pBase->setPropertyValue_NoBroadcast_public(
        st.NAME, makeAny( name ) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.TYPE, makeAny( xRow->getInt( DATA_TYPE ) ) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.TYPE_NAME, makeAny( typeName ) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.PRECISION, makeAny( xRow->getInt( COLUMN_SIZE ) ) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.SCALE, makeAny( xRow->getInt( DECIMAL_DIGITS ) ) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.IS_NULLABLE, makeAny( xRow->getInt( IS_NULLABLE ) ) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.DEFAULT_VALUE, makeAny( xRow->getString( DEFAULT_VALUE ) ) );

//     pBase->setPropertyValue_NoBroadcast_public(
//         st.DESCRIPTION, makeAny( xRow->getString( DESCRIPTION ) ) );

//     if( pBase->getPropertySetInfo()->hasPropertyByName( st.HELP_TEXT ) )
//         pBase->setPropertyValue_NoBroadcast_public(
//             st.HELP_TEXT, makeAny( xRow->getString( DESCRIPTION ) ) );
//     else // for key columns, etc. ...
        pBase->setPropertyValue_NoBroadcast_public(
            st.DESCRIPTION, makeAny( xRow->getString( DESCRIPTION ) ) );


    // maybe a better criterion than the type name can be found in future
    pBase->setPropertyValue_NoBroadcast_public(
        st.IS_AUTO_INCREMENT, isAutoIncrement(xRow->getString( DEFAULT_VALUE )) );

    pBase->setPropertyValue_NoBroadcast_public(
        st.IS_CURRENCY, isCurrency( typeName));
    return name;
}


// class CommentChanger : public cppu::WeakImplHelper< XPropertyChangeListener >
// {
//     ::rtl::Reference< RefCountedMutex > m_refMutex;
//     ::com::sun::star::uno::Reference< com::sun::star::sdbc::XConnection > m_connection;
//     ConnectionSettings *m_pSettings;
//     OUString m_schema;
//     OUString m_table;
//     OUString m_column;

// public:
//     CommentChanger(
//         const ::rtl::Reference< RefCountedMutex > & refMutex,
//         const ::com::sun::star::uno::Reference< com::sun::star::sdbc::XConnection > & connection,
//         ConnectionSettings *pSettings,
//         const OUString & schema,
//         const OUString & table,
//         const OUString & column ) :
//         m_refMutex( refMutex ),
//         m_connection( connection ),
//         m_pSettings( pSettings ),
//         m_schema ( schema ),
//         m_table ( table ),
//         m_column ( column )
//     {}


//     // Methods
//     virtual void SAL_CALL disposing( const ::com::sun::star::lang::EventObject& Source ) throw (::com::sun::star::uno::RuntimeException)
//     {
//         osl::MutexGuard guard( m_refMutex->mutex );
//         m_connection.clear();
//     }
//         // Methods
//     virtual void SAL_CALL propertyChange( const ::com::sun::star::beans::PropertyChangeEvent& evt ) throw (::com::sun::star::uno::RuntimeException)
//     {
//         osl::MutexGuard guard( m_refMutex->mutex );
//         OUStringBuffer buf( 128 );
//         OUString comment;
//         evt.NewValue >>= comment;
//         buf.append( "COMMENT ON COLUMN" );
//         bufferQuoteQualifiedIdentifier( buf, m_schema, m_table , m_column );
//         buf.append( "IS " );
//         bufferQuoteConstant( buf, comment,m_pSettings->encoding);

//         printf( "changing comment of column %s to %s\n",
//                 OUStringToOString( m_column, RTL_TEXTENCODING_ASCII_US ).getStr(),
//                 OUStringToOString( comment, RTL_TEXTENCODING_ASCII_US ).getStr() );

//         m_connection->createStatement()->executeUpdate( buf.makeStringAndClear() );
//     }
// };

void Columns::refresh()
    throw (::com::sun::star::uno::RuntimeException, std::exception)
{
    try
    {
        if( isLog( m_pSettings, LogLevel::INFO ) )
        {
            OStringBuffer buf;
            buf.append( "sdbcx.Columns get refreshed for table " );
            buf.append( OUStringToOString( m_schemaName, m_pSettings->encoding ) );
            buf.append( "." );
            buf.append( OUStringToOString( m_tableName, m_pSettings->encoding ) );
            log( m_pSettings, LogLevel::INFO, buf.makeStringAndClear().getStr() );
        }
        osl::MutexGuard guard( m_refMutex->mutex );

        Statics &st = getStatics();
        Reference< XDatabaseMetaData > meta = m_origin->getMetaData();

        Reference< XResultSet > rs =
            meta->getColumns( Any(), m_schemaName, m_tableName, st.cPERCENT );

        DisposeGuard disposeIt( rs );
        Reference< XRow > xRow( rs , UNO_QUERY );

        String2IntMap map;

        m_values.clear();
        int columnIndex = 0;
        while( rs->next() )
        {
            Column * pColumn =
                new Column( m_refMutex, m_origin, m_pSettings );
            Reference< com::sun::star::beans::XPropertySet > prop = pColumn;

            OUString name = columnMetaData2SDBCX( pColumn, xRow );
//             pColumn->addPropertyChangeListener(
//                 st.HELP_TEXT,
//                 new CommentChanger(
//                     m_refMutex,
//                     m_origin,
//                     m_pSettings,
//                     m_schemaName,
//                     m_tableName,
//                     name ) );

            {
                m_values.push_back( makeAny( prop ) );
                map[ name ] = columnIndex;
                ++columnIndex;
            }
        }
        m_name2index.swap( map );
    }
    catch ( com::sun::star::sdbc::SQLException & e )
    {
        throw RuntimeException( e.Message , e.Context );
    }
    fire( RefreshedBroadcaster( *this ) );
}


void alterColumnByDescriptor(
    const OUString & schemaName,
    const OUString & tableName,
    ConnectionSettings *settings,
    const Reference< XStatement > &stmt,
    const com::sun::star::uno::Reference< com::sun::star::beans::XPropertySet > & past,
    const com::sun::star::uno::Reference< com::sun::star::beans::XPropertySet > & future)
{
    Statics & st  = getStatics();

//     if( past->getPropertyValue( st.TABLE_NAME ) != future->getPropertyValue( st.TABLE_NAME ) ||
//         past->getPropertyValue( st.SCHEMA_NAME ) != future->getPropertyValue( st.SCHEMA_NAME ))
//     {
//         OUStringBuffer buf(128);
//         buf.append( "Can't move column " );
//         buf.append( extractStringProperty( past, st.COLUMN_NAME ) );
//         buf.append( " from table " );
//         buf.append( extractStringProperty( past, st.TABLE_NAME ) );
//         buf.append( " to table " );
//         buf.append( extractStringProperty( past, st.TABLE_NAME ) );
//         throw SQLException( buf.makeStringAndClear() );
//     }

//     OUString tableName = extractStringProperty( past, st.TABLE_NAME );
//     OUString schemaName = extractStringProperty( past, st.SCHEMA_NAME );
    OUString pastColumnName = extractStringProperty( past, st.NAME );
    OUString futureColumnName = extractStringProperty( future, st.NAME );
    OUString pastTypeName = sqltype2string( past );
    OUString futureTypeName = sqltype2string( future );

    TransactionGuard transaction( stmt );

    OUStringBuffer buf( 128 );
    if( ! pastColumnName.getLength())
    {
        // create a new column
        buf.append( "ALTER TABLE" );
        bufferQuoteQualifiedIdentifier( buf, schemaName, tableName, settings );
        buf.append( "ADD COLUMN" );
        bufferQuoteIdentifier( buf, futureColumnName, settings );
        buf.append( futureTypeName );
        transaction.executeUpdate( buf.makeStringAndClear() );
    }
    else
    {
        if( pastTypeName != futureTypeName )
        {
            throw RuntimeException(
                "Can't modify column types, drop the column and create a new one" );
        }

        if( pastColumnName != futureColumnName )
        {
            buf.append( "ALTER TABLE" );
            bufferQuoteQualifiedIdentifier( buf, schemaName, tableName, settings );
            buf.append( "RENAME COLUMN" );
            bufferQuoteIdentifier( buf, pastColumnName, settings );
            buf.append( "TO" );
            bufferQuoteIdentifier( buf, futureColumnName, settings );
            transaction.executeUpdate( buf.makeStringAndClear() );
        }
    }

    OUString futureDefaultValue = extractStringProperty( future, st.DEFAULT_VALUE );
    OUString pastDefaultValue = extractStringProperty( past, st.DEFAULT_VALUE );
    if( futureDefaultValue != pastDefaultValue )
    {
        buf = OUStringBuffer( 128 );
        buf.append( "ALTER TABLE" );
        bufferQuoteQualifiedIdentifier( buf, schemaName, tableName, settings );
        buf.append( "ALTER COLUMN" );
        bufferQuoteIdentifier( buf, futureColumnName, settings );
        buf.append( "SET DEFAULT " );
        // LEM TODO: check out
        // default value is not quoted, caller needs to quote himself (otherwise
        // how to pass e.g. nextval('something' ) ????
        buf.append( futureDefaultValue );
//        bufferQuoteConstant( buf, defaultValue, encoding );
        transaction.executeUpdate( buf.makeStringAndClear() );
    }

    sal_Int32 futureNullable = extractIntProperty( future, st.IS_NULLABLE );
    sal_Int32 pastNullable = extractIntProperty( past, st.IS_NULLABLE );
    if( futureNullable != pastNullable )
    {
        buf = OUStringBuffer( 128 );
        buf.append( "ALTER TABLE" );
        bufferQuoteQualifiedIdentifier( buf, schemaName, tableName, settings );
        buf.append( "ALTER COLUMN" );
        bufferQuoteIdentifier( buf, futureColumnName, settings );
        if( futureNullable == com::sun::star::sdbc::ColumnValue::NO_NULLS )
        {
            buf.append( "SET" );
        }
        else
        {
            buf.append( "DROP" );
        }
        buf.append( " NOT NULL" );
        transaction.executeUpdate( buf.makeStringAndClear() );
    }

//     OUString futureComment = extractStringProperty( future, st.HELP_TEXT );
//     OUString pastComment = extractStringProperty( past, st.HELP_TEXT );
//     printf( "past Comment %s, futureComment %s\n",
//             OUStringToOString( pastComment, RTL_TEXTENCODING_ASCII_US ).getStr(),
//             OUStringToOString( futureComment, RTL_TEXTENCODING_ASCII_US ).getStr() );
    OUString futureComment = extractStringProperty( future, st.DESCRIPTION );
    OUString pastComment = extractStringProperty( past, st.DESCRIPTION );

    if( futureComment != pastComment )
    {
        buf = OUStringBuffer( 128 );
        buf.append( "COMMENT ON COLUMN" );
        bufferQuoteQualifiedIdentifier( buf, schemaName, tableName , futureColumnName, settings );
        buf.append( "IS " );
        bufferQuoteConstant( buf, futureComment, settings );
        transaction.executeUpdate( buf.makeStringAndClear() );
    }
    transaction.commit();
}

void Columns::appendByDescriptor(
    const ::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertySet >& future )
    throw (::com::sun::star::sdbc::SQLException,
           ::com::sun::star::container::ElementExistException,
           ::com::sun::star::uno::RuntimeException, std::exception)
{
    osl::MutexGuard guard( m_refMutex->mutex );
    Statics & st = getStatics();
    Reference< XPropertySet > past = createDataDescriptor();
    past->setPropertyValue( st.IS_NULLABLE, makeAny( com::sun::star::sdbc::ColumnValue::NULLABLE ) );
    alterColumnByDescriptor(
        m_schemaName, m_tableName, m_pSettings, m_origin->createStatement() , past, future  );

    refresh();
}

// void Columns::dropByName( const OUString& elementName )
//     throw (::com::sun::star::sdbc::SQLException,
//            ::com::sun::star::container::NoSuchElementException,
//            ::com::sun::star::uno::RuntimeException)
// {
//     String2IntMap::const_iterator ii = m_name2index.find( elementName );
//     if( ii == m_name2index.end() )
//     {
//         OUStringBuffer buf( 128 );
//         buf.appendAscii( "Column " );
//         buf.append( elementName );
//         buf.appendAscii( " is unknown in table " );
//         buf.append( m_schemaName );
//         buf.appendAscii( "." );
//         buf.append( m_tableName );
//         buf.appendAscii( ", so it can't be dropped" );
//         throw com::sun::star::container::NoSuchElementException(
//             buf.makeStringAndClear(), *this );
//     }
//     dropByIndex( ii->second );
// }

void Columns::dropByIndex( sal_Int32 index )
    throw (::com::sun::star::sdbc::SQLException,
           ::com::sun::star::lang::IndexOutOfBoundsException,
           ::com::sun::star::uno::RuntimeException, std::exception)
{
    osl::MutexGuard guard( m_refMutex->mutex );
    if( index < 0 ||  index >= (sal_Int32)m_values.size() )
    {
        OUStringBuffer buf( 128 );
        buf.append( "COLUMNS: Index out of range (allowed 0 to " );
        buf.append((sal_Int32)(m_values.size() -1) );
        buf.append( ", got " );
        buf.append( index );
        buf.append( ")" );
        throw com::sun::star::lang::IndexOutOfBoundsException(
            buf.makeStringAndClear(), *this );
    }

    Reference< XPropertySet > set;
    m_values[index] >>= set;
    Statics &st = getStatics();
    OUString name;
    set->getPropertyValue( st.NAME ) >>= name;

    OUStringBuffer update( 128 );
    update.append( "ALTER TABLE ONLY");
    bufferQuoteQualifiedIdentifier( update, m_schemaName, m_tableName, m_pSettings );
    update.append( "DROP COLUMN" );
    bufferQuoteIdentifier( update, name, m_pSettings );
    Reference< XStatement > stmt = m_origin->createStatement( );
    DisposeGuard disposeIt( stmt );
    stmt->executeUpdate( update.makeStringAndClear() );

    Container::dropByIndex( index );
}


::com::sun::star::uno::Reference< ::com::sun::star::beans::XPropertySet > Columns::createDataDescriptor()
        throw (::com::sun::star::uno::RuntimeException, std::exception)
{
    return new ColumnDescriptor( m_refMutex, m_origin, m_pSettings );
}

Reference< com::sun::star::container::XNameAccess > Columns::create(
    const ::rtl::Reference< RefCountedMutex > & refMutex,
    const ::com::sun::star::uno::Reference< com::sun::star::sdbc::XConnection >  & origin,
    ConnectionSettings *pSettings,
    const OUString &schemaName,
    const OUString &tableName,
    Columns **ppColumns)
{
    *ppColumns = new Columns(
        refMutex, origin, pSettings, schemaName, tableName );
    Reference< com::sun::star::container::XNameAccess > ret = *ppColumns;
    (*ppColumns)->refresh();

    return ret;
}


ColumnDescriptors::ColumnDescriptors(
        const ::rtl::Reference< RefCountedMutex > & refMutex,
        const ::com::sun::star::uno::Reference< com::sun::star::sdbc::XConnection >  & origin,
        ConnectionSettings *pSettings )
    : Container( refMutex, origin, pSettings,  "COLUMN-DESCRIPTOR" )
{}


Reference< ::com::sun::star::beans::XPropertySet > ColumnDescriptors::createDataDescriptor()
        throw (::com::sun::star::uno::RuntimeException, std::exception)
{
    return new ColumnDescriptor( m_refMutex, m_origin, m_pSettings );
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
