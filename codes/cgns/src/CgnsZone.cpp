/*---------------------------------------------------------------------------*\
    OneFLOW - LargeScale Multiphysics Scientific Simulation Environment
    Copyright (C) 2017-2020 He Xin and the OneFLOW contributors.
-------------------------------------------------------------------------------
License
    This file is part of OneFLOW.

    OneFLOW is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OneFLOW is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OneFLOW.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "CgnsZone.h"
#include "CgnsBase.h"
#include "CgnsCoor.h"
#include "CgnsSection.h"
#include "CgnsMultiSection.h"
#include "CgnsBcRegion.h"
#include "CgnsBcRegionProxy.h"
#include "BcRecord.h"
#include "Boundary.h"
#include "NodeMesh.h"
#include "StrUtil.h"
#include "Grid.h"
#include "BgGrid.h"
#include "StrGrid.h"
#include "GridState.h"
#include "Dimension.h"
#include "GridElem.h"
#include "ElemFeature.h"
#include "ElementHome.h"
#include "PointFactory.h"
#include "PointSearch.h"
#include "FaceSolver.h"
#include "Stop.h"
#include <iostream>
#include <iomanip>
using namespace std;

BeginNameSpace( ONEFLOW )
#ifdef ENABLE_CGNS

CgnsZone::CgnsZone( CgnsBase * cgnsBase )
{
    this->cgnsBase = cgnsBase;
    this->nodeMesh = 0;
    this->multiSection = 0;
    this->bcRegionProxy = 0;
    this->volBcType = -1;
}

CgnsZone::~CgnsZone()
{
    delete this->nodeMesh;
    delete this->multiSection;
    delete this->bcRegionProxy;
}

void CgnsZone::FreeMesh()
{
    delete this->nodeMesh;
    this->nodeMesh = 0;
}

void CgnsZone::SetVolBcType( int volBcType )
{
    this->volBcType = volBcType;
}

int CgnsZone::GetVolBcType()
{
    return this->volBcType;
}

void CgnsZone::Create()
{
    this->nodeMesh = new NodeMesh();
    this->multiSection = new CgnsMultiSection( this );
    this->bcRegionProxy = new CgnsBcRegionProxy( this );
}

void CgnsZone::InitElement( GridElem * ge )
{
    this->ConstructCgnsGridPoints( ge->point_factory );
    this->SetElementTypeAndNode( ge->elem_feature );
}

void CgnsZone::SetPeriodicBc()
{
    this->bcRegionProxy->SetPeriodicBc();
}

void CgnsZone::SetElementTypeAndNode( ElemFeature * elem_feature )
{
    size_t nSection = this->multiSection->nSection;
    for ( int iSection = 0; iSection < nSection; ++ iSection )
    {
        CgnsSection * cgnsSection = this->multiSection->GetCgnsSection( iSection );
        cgnsSection->SetElementTypeAndNode( elem_feature );
    }
    cout << "\n";
    cout << " iZone = " << this->zId << " nCell = " << nCell << "\n";
    cout << " elem_feature->eType->size = " << elem_feature->eType->size() << endl;
}

void CgnsZone::InitLgMapping()
{
    this->l2g.resize( this->nNode );

    for ( int iNode = 0; iNode < this->nNode; ++ iNode )
    {
        this->l2g[ iNode ] = iNode;
    }
}

void CgnsZone::ConvertToInnerDataStandard()
{
    if ( this->cgnsZoneType == Structured )
    {
        //this->SetStructuredSectionInformation();
        return;
    }

    this->multiSection->ConvertToInnerDataStandard();
    this->bcRegionProxy->ConvertToInnerDataStandard();

}

void CgnsZone::ConstructCgnsGridPoints( PointFactory * point_factory )
{
    RealField & x = this->nodeMesh->xN;
    RealField & y = this->nodeMesh->yN;
    RealField & z = this->nodeMesh->zN;

    this->InitLgMapping();

    size_t nNode = this->nodeMesh->GetNumberOfNodes();

    for ( int iNode = 0; iNode < nNode; ++ iNode )
    {
        int pid = point_factory->AddPoint( x[ iNode ], y[ iNode ], z[ iNode ] );
        
        this->l2g[ iNode ] = pid; //For the merging of partitioned multiple blocks with single block
    }
}

void CgnsZone::ScanBcFace( FaceSolver * face_solver )
{
    this->bcRegionProxy->ScanBcFace( face_solver );
}

void CgnsZone::GetElementNodeId( CgInt eId, CgIntField & eNodeId )
{
    CgnsSection * cgnsSection = this->multiSection->GetSectionByEid( eId );
    cgnsSection->GetElementNodeId( eId - cgnsSection->startId, eNodeId );
}

void CgnsZone::FillISize( Grid * gridIn )
{
    StrGrid * grid = ONEFLOW::StrGridCast( gridIn );
    int ni = grid->ni;
    int nj = grid->nj;
    int nk = grid->nk;
    this->FillISize( ni, nj, nk, THREE_D );
    //if ( Dim::dimension == THREE_D )
    //{
    //    this->FillISize( ni, nj, nk, THREE_D );
    //}
    //else
    //{
    //    this->FillISize( ni, nj, nk, Dim::dimension );
    //}
}

void CgnsZone::FillISize( int ni, int nj, int nk, int dimension )
{
    int j = 0;
    // vertex size
    isize[ j ++ ] = ni;
    isize[ j ++ ] = nj;
    if ( dimension == THREE_D )
    {
        isize[ j ++ ] = nk;
    }
    // cell size
    isize[ j ++ ] = ni - 1;
    isize[ j ++ ] = nj - 1;
    if ( dimension == THREE_D )
    {
        //isize[ j ++ ] = MAX( nk - 1, 1 );
        isize[ j ++ ] = nk - 1;
    }
    // boundary vertex size (always zero for structured grids)
    isize[ j ++ ] = 0;
    isize[ j ++ ] = 0;
    if ( dimension == THREE_D )
    {
        isize[ j ++ ] = 0;
    }
}

void CgnsZone::DumpCgnsZone( Grid * grid )
{
    this->DumpCgnsZoneAttribute( grid );

    this->DumpCgnsGridBoundary( grid );

    this->DumpCgnsGridCoordinates( grid );
}

void CgnsZone::ReadCgnsGrid()
{
    this->ReadCgnsZoneAttribute();

    this->ReadElementConnectivities();

    this->ReadCgnsGridBoundary();

    this->ReadCgnsGridCoordinates();

    this->ConvertToInnerDataStandard();
}

void CgnsZone::ReadCgnsGrid( CgnsZone * cgnsZoneIn )
{
    this->ReadCgnsZoneAttribute( cgnsZoneIn );

    this->ReadElementConnectivities( cgnsZoneIn );

    this->ReadCgnsGridCoordinates( cgnsZoneIn );

    this->ConvertToInnerDataStandard();
}

void CgnsZone::ReadCgnsZoneAttribute()
{
    this->ReadCgnsZoneType();

    this->ReadCgnsZoneNameAndGeneralizedDimension();

    this->SetDimension();
}

void CgnsZone::DumpCgnsZoneAttribute( Grid * grid )
{
    this->DumpCgnsZoneType( grid );

    this->DumpCgnsZoneNameAndGeneralizedDimension( grid );
}

void CgnsZone::ReadCgnsZoneAttribute( CgnsZone * cgnsZoneIn )
{
    this->ReadCgnsZoneType( cgnsZoneIn );

    this->ReadCgnsZoneNameAndGeneralizedDimension( cgnsZoneIn );

    this->SetDimension( cgnsZoneIn );
}

void CgnsZone::ReadCgnsZoneType()
{
    //Check the zone type
    cg_zone_type( cgnsBase->fileId, cgnsBase->baseId, this->zId, & cgnsZoneType );

    cout << "   The Zone Type is " << GetCgnsZoneTypeName( cgnsZoneType ) << " Zone" << "\n";
}

void CgnsZone::DumpCgnsZoneType( Grid * grid )
{
    if ( IsUnsGrid( grid->type ) )
    {
        this->cgnsZoneType = Unstructured;
    }
    else
    {
        this->cgnsZoneType = Structured;
    }

    cout << "   The Zone Type is " << GetCgnsZoneTypeName( cgnsZoneType ) << " Zone" << "\n";
}

void CgnsZone::ReadCgnsZoneType( CgnsZone * cgnsZoneIn )
{
    this->cgnsZoneType = Unstructured;

    cout << "   The Zone Type is " << GetCgnsZoneTypeName( cgnsZoneType ) << " Zone" << "\n";
}

void CgnsZone::ReadCgnsZoneNameAndGeneralizedDimension()
{
    CgnsTraits::char33 cgnsZoneName;

    //Determine the number of vertices and cellVolume elements in this zone
    cg_zone_read( cgnsBase->fileId, cgnsBase->baseId, this->zId, cgnsZoneName, this->isize );

    this->zoneName = cgnsZoneName;

    cout << "   CGNS Zone Name = " << cgnsZoneName << "\n";
}

void CgnsZone::DumpCgnsZoneNameAndGeneralizedDimension( Grid * gridIn )
{
    this->FillISize( gridIn );

    this->zoneName = gridIn->name;
    this->zId = -1;
    cout << " cell dim = " << this->cgnsBase->celldim << " physics dim = " << this->cgnsBase->phydim << "\n";
    //create zone
    cg_zone_write(cgnsBase->fileId, cgnsBase->baseId, this->zoneName.c_str(), this->isize, this->cgnsZoneType, &this->zId );
    cout << " Zone Id = " << this->zId << "\n";

    cout << "   CGNS Zone Name = " << zoneName << "\n";
}

void CgnsZone::ReadCgnsZoneNameAndGeneralizedDimension( CgnsZone * cgnsZoneIn )
{
    this->zoneName = cgnsZoneIn->zoneName;
}

void CgnsZone::SetDimension()
{
    if ( this->cgnsZoneType == Structured )
    {
        // lower range index
        irmin[ 0 ] = 1;
        irmin[ 1 ] = 1;
        irmin[ 2 ] = 1;

        // upper range index of vertices
        irmax[ 0 ] = 1;
        irmax[ 1 ] = 1;
        irmax[ 2 ] = 1;

        cellSize[ 0 ] = 1;
        cellSize[ 1 ] = 1;
        cellSize[ 2 ] = 1;

        // upper range index of vertices
        // vertex size
        int j = 0;
        irmax[ 0 ] = isize[ j ++ ];
        irmax[ 1 ] = isize[ j ++ ];
        if ( this->cgnsBase->celldim == THREE_D )
        {
            irmax[ 2 ] = isize[ j ++ ];
        }
        // cell size
        cellSize[ 0 ] = isize[ j ++ ];
        cellSize[ 1 ] = isize[ j ++ ];
        if ( this->cgnsBase->celldim == THREE_D )
        {
            cellSize[ 2 ] = isize[ j ++ ];
        }
        cout << "   The Dimension Of Grid is : \n";
        cout << "   I Direction " << setw( 10 ) << irmin[ 0 ] << setw( 10 ) << irmax[ 0 ] << "\n";
        cout << "   J Direction " << setw( 10 ) << irmin[ 1 ] << setw( 10 ) << irmax[ 1 ] << "\n";
        cout << "   K Direction " << setw( 10 ) << irmin[ 2 ] << setw( 10 ) << irmax[ 2 ] << "\n";
        this->nNode = irmax[ 0 ] * irmax[ 1 ] * irmax[ 2 ];
        this->nCell = cellSize[ 0 ] * cellSize[ 1 ] * cellSize[ 2 ];
    }
    else
    {
        irmin[ 0 ] = 1;
        irmin[ 1 ] = 0;
        irmin[ 2 ] = 0;

        irmax[ 0 ] = isize[ 0 ];
        irmax[ 1 ] = 0;
        irmax[ 2 ] = 0;

        cellSize[ 0 ] = isize[ 1 ];

        this->nNode = irmax[ 0 ];
        this->nCell = cellSize[ 0 ];
    }

    cout << "   numberOfNodes = " << this->nNode << " numberOfCells = " << this->nCell << "\n";
}

void CgnsZone::SetDimension( CgnsZone * cgnsZoneIn )
{
    isize[ 0 ] = cgnsZoneIn->nNode;
    isize[ 1 ] = cgnsZoneIn->nCell;

    irmin[ 0 ] = 1;
    irmin[ 1 ] = 0;
    irmin[ 2 ] = 0;

    irmax[ 0 ] = isize[ 0 ];
    irmax[ 1 ] = 0;
    irmax[ 2 ] = 0;

    cellSize[ 0 ] = isize[ 1 ];

    this->nNode = irmax[ 0 ];
    this->nCell = cellSize[ 0 ];

    this->InitL2g();

    cout << "   numberOfNodes = " << this->nNode << " numberOfCells = " << this->nCell << "\n";
}

void CgnsZone::InitL2g()
{
    l2g.resize( this->nNode );

    for ( int iNode = 0; iNode < this->nNode; ++ iNode )
    {
        l2g[ iNode ] = iNode;
    }
}

CgInt CgnsZone::GetNI() const { return irmax[0]; };
CgInt CgnsZone::GetNJ() const { return irmax[1]; };
CgInt CgnsZone::GetNK() const { return irmax[2]; };

void CgnsZone::ReadElementConnectivities()
{
    if ( this->cgnsZoneType == Structured ) return;

    this->ReadNumberOfCgnsSections();

    this->CreateCgnsSections();

    this->ReadCgnsSections();
}

void CgnsZone::ReadElementConnectivities( CgnsZone * cgnsZoneIn )
{
    this->AllocateUnsElemConn   ( cgnsZoneIn );
    this->GenerateUnsVolElemConn( cgnsZoneIn );
    this->GenerateUnsBcElemConn ( cgnsZoneIn );
    this->SetElemPosition();
    this->GenerateUnsBcCondConn ( cgnsZoneIn );
}

void CgnsZone::AllocateUnsElemConn( CgnsZone * cgnsZoneIn )
{
    this->multiSection->nSection = 2;
    this->multiSection->CreateCgnsSection();

    int s1, e1, s2, e2, etype1, etype2;
    cgnsZoneIn->GetStrZonePara( s1, e1, s2, e2, etype1, etype2 );

    CgnsSection * cgnsSection1 = this->multiSection->GetCgnsSection( 0 );
    CgnsSection * cgnsSection2 = this->multiSection->GetCgnsSection( 1 );
    cgnsSection1->SetSectionInfo( "Section1", etype1, s1, e1 );
    cgnsSection2->SetSectionInfo( "Section2", etype2, s2, e2 );

    this->multiSection->CreateConnList();
}

void CgnsZone::GetStrZonePara( int & s1, int & e1, int & s2, int & e2, int & etype1, int & etype2  )
{
   int nActualBcFace = this->bcRegionProxy->GetNumberOfActualBcElements();

    s1 = 1;
    e1 = this->nCell;

    s2 = e1 + 1;
    e2 = e1 + nActualBcFace;

    int celldim = this->cgnsBase->celldim;

    if ( celldim == ONE_D )
    {
        etype1  = CGNS_ENUMV( BAR_2 );
        etype2  = CGNS_ENUMV( NODE );
    }
    else if ( celldim == TWO_D )
    {
        etype1  = CGNS_ENUMV( QUAD_4 );
        etype2  = CGNS_ENUMV( BAR_2  );
    }
    else if ( celldim == THREE_D )
    {
        etype1  = CGNS_ENUMV( HEXA_8 );
        etype2  = CGNS_ENUMV( QUAD_4 );
    }
}

void CgnsZone::SetElemPosition()
{
    this->multiSection->SetElemPosition();
}

void CgnsZone::GenerateUnsVolElemConn( CgnsZone * cgnsZoneIn )
{
    int ni = static_cast<int> (cgnsZoneIn->GetNI());
    int nj = static_cast<int> (cgnsZoneIn->GetNJ());
    int nk = static_cast<int> (cgnsZoneIn->GetNK());

    cout << " ni = " << ni << " nj = " << nj << " nk = " << nk << "\n";

    int iSection = 0;
    CgnsSection * cgnsSection = this->multiSection->GetCgnsSection( iSection );

    Range I, J, K;
    GetRange( ni, nj, nk, 0, -1, I, J, K );

    int ist, ied, jst, jed, kst, ked;
    GetIJKRegion( I, J, K, ist, ied, jst, jed, kst, ked );

    int il1 = 1;
    int jl1 = 1;
    int kl1 = 1;

    int cell_dim = this->cgnsBase->celldim;

    if ( cell_dim == TWO_D ) kl1 = 0;
    if ( cell_dim == ONE_D ) jl1 = 0;

    CgIntField & connList = cgnsSection->connList;

    int pos = 0;

    for ( int k = kst; k <= ked; ++ k )
    {
        for ( int j = jst; j <= jed; ++ j )
        {
            for ( int i = ist; i <= ied; ++ i )
            {
                int index1, index2, index3, index4;
                EncodeIJK( index1,  i      , j      , k,  ni,  nj,  nk );
                EncodeIJK( index2,  i + il1, j      , k,  ni,  nj,  nk );

                connList[ pos ++ ] = this->l2g[ index1 ] + 1;
                connList[ pos ++ ] = this->l2g[ index2 ] + 1;

                if ( cell_dim == ONE_D ) continue;

                EncodeIJK( index3,  i + il1, j + jl1, k,  ni,  nj,  nk );
                EncodeIJK( index4,  i      , j + jl1, k,  ni,  nj,  nk );

                connList[ pos ++ ] = this->l2g[ index3 ] + 1;
                connList[ pos ++ ] = this->l2g[ index4 ] + 1;

                if ( cell_dim == TWO_D ) continue;

                int index5, index6, index7, index8;
                EncodeIJK( index5,  i      , j      , k + kl1,  ni,  nj,  nk );
                EncodeIJK( index6,  i + il1, j      , k + kl1,  ni,  nj,  nk );
                EncodeIJK( index7,  i + il1, j + jl1, k + kl1,  ni,  nj,  nk );
                EncodeIJK( index8,  i      , j + jl1, k + kl1,  ni,  nj,  nk );

                connList[ pos ++ ] = this->l2g[ index5 ] + 1;
                connList[ pos ++ ] = this->l2g[ index6 ] + 1;
                connList[ pos ++ ] = this->l2g[ index7 ] + 1;
                connList[ pos ++ ] = this->l2g[ index8 ] + 1;
            }
        }
    }
}

void CgnsZone::GenerateUnsBcElemConn( CgnsZone * cgnsZoneIn )
{
    int iSection = 1;
    CgnsSection * cgnsSection = this->multiSection->GetCgnsSection( iSection );

    this->CreateCgnsBcRegion( cgnsZoneIn );

    cout << " ConnectionList Size = " << cgnsSection->connSize << "\n";
    cgnsZoneIn->bcRegionProxy->GenerateUnsBcElemConn( cgnsSection->connList );
}

void CgnsZone::GenerateUnsBcCondConn( CgnsZone * cgnsZoneIn )
{
    int nBcRegion = cgnsZoneIn->bcRegionProxy->nBcRegion;

    int iSection = 1;
    CgnsSection * cgnsSection = this->multiSection->GetCgnsSection( iSection );

    CgInt startId = cgnsSection->startId;

    for ( int iBcRegion = 0; iBcRegion < nBcRegion; ++ iBcRegion )
    {
        CgnsBcRegion * bcRegion    = this      ->bcRegionProxy->GetBcRegion( iBcRegion );
        CgnsBcRegion * strBcRegion = cgnsZoneIn->bcRegionProxy->GetBcRegion( iBcRegion );
        bcRegion->CopyStrBcRegion( strBcRegion, startId );
    }
}

void CgnsZone::ReadNumberOfCgnsSections()
{
    this->multiSection->ReadNumberOfCgnsSections();
}

void CgnsZone::ReadNumberOfCgnsSections( CgnsZone * cgnsZoneIn )
{
    this->multiSection->nSection = cgnsZoneIn->multiSection->nSection;
    cout << "   numberOfCgnsSections = " << this->multiSection->nSection << "\n";
}

void CgnsZone::CreateCgnsSections()
{
    this->multiSection->CreateCgnsSection();
}

void CgnsZone::ReadCgnsSections()
{
    this->multiSection->ReadCgnsSections();
}

void CgnsZone::ReadCgnsGridCoordinates()
{
    //Determine the number and names of the coordinates.
    int fileId = cgnsBase->fileId;
    int baseId = cgnsBase->baseId;
    cg_ncoords( fileId, baseId, this->zId, & this->nCoor );

    nodeMesh->CreateNodes( static_cast<int>(this->nNode));

    CgnsCoor * cgnsCoor = new CgnsCoor();

    for ( int coordId = 1; coordId <= this->nCoor; ++ coordId )
    {
        DataType_t dataType;
        CgnsTraits::char33 coorName;
        cg_coord_info( fileId, baseId, this->zId, coordId, & dataType, coorName );
        int coId = coordId - 1;
        cgnsCoor->Alloc( coId, static_cast<int>(this->nNode), dataType );
        //Read the x-, y-, z-coordinates.
        cg_coord_read( fileId, baseId, this->zId, coorName, dataType, this->irmin, this->irmax, cgnsCoor->GetCoor( coId ) );
    }

    cgnsCoor->SetAllData( nodeMesh->xN, nodeMesh->yN, nodeMesh->zN );

    delete cgnsCoor;
}

void CgnsZone::DumpCgnsGridCoordinates( Grid * grid )
{
    // write grid coordinates (user must use SIDS-standard names here)
    int index_x = -1;
    int index_y = -2;
    int index_z = -3;
    cg_coord_write( cgnsBase->fileId, cgnsBase->baseId, this->zId, RealDouble, "CoordinateX", &grid->nodeMesh->xN[0], &index_x );
    cg_coord_write( cgnsBase->fileId, cgnsBase->baseId, this->zId, RealDouble, "CoordinateY", &grid->nodeMesh->yN[0], &index_y );
    cg_coord_write( cgnsBase->fileId, cgnsBase->baseId, this->zId, RealDouble, "CoordinateZ", &grid->nodeMesh->zN[0], &index_z );
    cout << " index_x = " << index_x << "\n";
    cout << " index_y = " << index_y << "\n";
    cout << " index_z = " << index_z << "\n";
}

void CgnsZone::ReadCgnsGridCoordinates( CgnsZone * cgnsZoneIn )
{
    * this->nodeMesh = * cgnsZoneIn->nodeMesh;
}

void CgnsZone::ReadCgnsGridBoundary()
{
    bcRegionProxy->ReadCgnsGridBoundary();
}

void CgnsZone::DumpCgnsGridBoundary( Grid * grid )
{
    bcRegionProxy->DumpCgnsGridBoundary( grid );
}

void CgnsZone::ProcessPeriodicBc()
{
    ;
}

void CgnsZone::CreateCgnsBcRegion( CgnsZone * cgnsZoneIn )
{
    bcRegionProxy->CreateCgnsBcRegion( cgnsZoneIn->bcRegionProxy );
}

void CgnsZone::PrepareCgnsZone( Grid * grid )
{
    Grids grids;
    grids.push_back( grid );
    this->cgnsZoneType = Unstructured;
    ONEFLOW::PrepareCgnsZone( grids, this );
}

void EncodeIJK( int & index, int i, int j, int k, int ni, int nj, int nk )
{
    index = ( i - 1 ) + ( j - 1 ) * ni + ( k - 1 ) * ( ni * nj ) ;
}

void DecodeIJK( int index, int & i, int & j, int & k, int ni, int nj, int nk )
{
    k = index / ( ni * nj ) + 1;
    index -= ( k - 1 ) * ni * nj;
    j = index / ni + 1;
    i = index - ( j - 1 ) * ni + 1;
}

void GetRange( int ni, int nj, int nk, int startShift, int endShift, Range & I, Range & J, Range & K )
{
    I.SetRange( 1 + startShift, ni + endShift );
    J.SetRange( 1 + startShift, nj + endShift );
    K.SetRange( 1 + startShift, nk + endShift );

    if ( ni == 1 ) I.SetRange( 1, 1 );
    if ( nj == 1 ) J.SetRange( 1, 1 );
    if ( nk == 1 ) K.SetRange( 1, 1 );
}

void GetIJKRegion( Range & I, Range & J, Range & K, int & ist, int & ied, int & jst, int & jed, int & kst, int & ked )
{
    ist = I.First();
    ied = I.Last();
    jst = J.First();
    jed = J.Last();
    kst = K.First();
    ked = K.Last();
}

void PrepareCgnsZone( Grids & grids, CgnsZone * cgnsZone )
{
    NodeMesh * nodeMesh = cgnsZone->nodeMesh;

    int nNode, nCell;

    HXVector< Int3D * > unsIdList;

    MergeToSingleZone( grids, unsIdList, nodeMesh, nNode, nCell );

    cgnsZone->nNode = nNode;
    cgnsZone->nCell = nCell;

    FillSection( grids, unsIdList, cgnsZone );

    cgnsZone->ConvertToInnerDataStandard();

    ONEFLOW::DeletePointer( unsIdList );
}

void MergeToSingleZone( Grids & grids, HXVector< Int3D * > & unsIdList, NodeMesh * nodeMesh, int & nNode, int & nCell )
{
    PointSearch * point_search = new PointSearch();
    point_search->Initialize( grids );

    size_t nZone = grids.size();

    unsIdList.resize( nZone );
    nCell = 0;
    for ( int iZone = 0; iZone < nZone; ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        int ni = grid->ni;
        int nj = grid->nj;
        int nk = grid->nk;
        nCell += grid->nCell;
        unsIdList[ iZone ] = new Int3D( Range( 1, ni ), Range( 1, nj ), Range( 1, nk ) );
        Int3D & unsId = * unsIdList[ iZone ];
        cout << " block = " << iZone + 1 << "\n";
        ComputeUnsId( grid, point_search, & unsId );
    }

    nNode = point_search->GetNPoint();

    cout << " First nNode = " << nNode << "\n";
    nodeMesh->xN.resize( nNode );
    nodeMesh->yN.resize( nNode );
    nodeMesh->zN.resize( nNode );
    for ( int i = 0; i < nNode; ++ i )
    {
        Real xm, ym, zm;
        point_search->GetPoint( i, xm, ym, zm );

        nodeMesh->xN[ i ] = xm;
        nodeMesh->yN[ i ] = ym;
        nodeMesh->zN[ i ] = zm;
    }
    delete point_search;
}

void FillSection( Grids & grids, HXVector< Int3D * > & unsIdList, CgnsZone * cgnsZone )
{
    int nTBcRegion = 0;

    int nTCell = 0;
    int nBFace = 0;

    for ( int iZone = 0; iZone < grids.size(); ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        Int3D & unsId = * unsIdList[ iZone ];

        nTCell += grid->ComputeNumberOfCell();

        BcRegionGroup * bcRegionGroup = grid->bcRegionGroup;
        size_t nBcRegions = bcRegionGroup->regions->size();

        for ( int ir = 0; ir < nBcRegions; ++ ir )
        {
            BcRegion * bcRegion = ( * bcRegionGroup->regions )[ ir ];
            //if ( BC::IsPoleBc( bcRegion->bcType ) ) continue;
            if ( BC::IsNotNormalBc( bcRegion->bcType ) ) continue;
            
            nBFace += bcRegion->ComputeRegionCells();
            nTBcRegion ++;
        }
    }

    cout << " nBFace = " << nBFace << "\n";

    int iZone = 0;

    //CgnsZone * cgnsZone = cgnsMultiBase->GetCgnsZone( iZone );
    
    cgnsZone->nCell = nTCell;

    cgnsZone->multiSection->nSection = 2;
    cgnsZone->multiSection->CreateCgnsSection();

    cgnsZone->multiSection->cgnsSections[ 0 ]->startId = 1;
    cgnsZone->multiSection->cgnsSections[ 0 ]->endId   = nTCell;

    cgnsZone->multiSection->cgnsSections[ 1 ]->startId = nTCell + 1;
    cgnsZone->multiSection->cgnsSections[ 1 ]->endId   = nTCell + 1 + nBFace;

    if ( Dim::dimension == ONEFLOW::THREE_D )
    {
        cgnsZone->multiSection->cgnsSections[ 0 ]->eType = HEXA_8;
        cgnsZone->multiSection->cgnsSections[ 1 ]->eType = QUAD_4;
    }
    else
    {
        cgnsZone->multiSection->cgnsSections[ 0 ]->eType = QUAD_4;
        cgnsZone->multiSection->cgnsSections[ 1 ]->eType = BAR_2;
    }

    cgnsZone->multiSection->CreateConnList();

    CgnsBcRegionProxy * bcRegionProxy = cgnsZone->bcRegionProxy;
    bcRegionProxy->nOrdinaryBcRegion = nTBcRegion;
    bcRegionProxy->n1To1 = 0;
    bcRegionProxy->nConn = 0;
    bcRegionProxy->CreateCgnsBcRegion();

    CgnsSection * secV = cgnsZone->multiSection->GetCgnsSection( 0 );
    CgnsSection * secB = cgnsZone->multiSection->GetCgnsSection( 1 );

    CgIntField& connList  = secV->connList;
    CgIntField& bConnList = secB->connList;

    int pos = 0;

    for ( int iZone = 0; iZone < grids.size(); ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        int ni = grid->ni;
        int nj = grid->nj;
        int nk = grid->nk;

        Int3D & unsId = * unsIdList[ iZone ];
        
        IJKRange::Compute( ni, nj, nk, 0, -1 );
        IJKRange::ToScalar();

        int is = 1;
        int js = 1;
        int ks = 1;

        if ( Dim::dimension == ONEFLOW::TWO_D ) ks = 0;

        int eNodeNumbers = ONEFLOW::GetElementNodeNumbers( secV->eType );

        for ( int k = IJKRange::kst; k <= IJKRange::ked; ++ k )
        {
            for ( int j = IJKRange::jst; j <= IJKRange::jed; ++ j )
            {
                for ( int i = IJKRange::ist; i <= IJKRange::ied; ++ i )
                {
                    connList[ pos + 0 ] = unsId( i   , j   , k    ) + 1;
                    connList[ pos + 1 ] = unsId( i+is, j   , k    ) + 1;
                    connList[ pos + 2 ] = unsId( i+is, j+js, k    ) + 1;
                    connList[ pos + 3 ] = unsId( i   , j+js, k    ) + 1;
                    if ( Dim::dimension == ONEFLOW::THREE_D )
                    {
                        connList[ pos + 4 ] = unsId( i   , j   , k+ks ) + 1;
                        connList[ pos + 5 ] = unsId( i+is, j   , k+ks ) + 1;
                        connList[ pos + 6 ] = unsId( i+is, j+js, k+ks ) + 1;
                        connList[ pos + 7 ] = unsId( i   , j+js, k+ks ) + 1;
                    }
                    pos += eNodeNumbers;
                }
            }
        }
    }

    secV->SetElemPosition();
    secB->SetElemPosition();

    int irc  = 0;
    int eIdPos  = nTCell;
    pos = 0;

    BcTypeMap * bcTypeMap = new BcTypeMap();
    bcTypeMap->Init();

    for ( int iZone = 0; iZone < grids.size(); ++ iZone )
    {
        StrGrid * grid = ONEFLOW::StrGridCast( grids[ iZone ] );
        int ni = grid->ni;
        int nj = grid->nj;
        int nk = grid->nk;

        Int3D & unsId = * unsIdList[ iZone ];

        BcRegionGroup * bcRegionGroup = grid->bcRegionGroup;
        size_t nBcRegions = bcRegionGroup->regions->size();

        for ( int ir = 0; ir < nBcRegions; ++ ir )
        {
            BcRegion * bcRegion = ( * bcRegionGroup->regions )[ ir ];

            //if ( BC::IsPoleBc( bcRegion->bcType ) ) continue;
            if ( BC::IsNotNormalBc( bcRegion->bcType ) ) continue;
            int nRegionCell = bcRegion->ComputeRegionCells();

            CgnsBcRegion * cgnsBcRegion = bcRegionProxy->cgnsBcRegions[ irc ];
            
            cgnsBcRegion->gridLocation = CellCenter;
            cgnsBcRegion->nElements    = 2;
            cgnsBcRegion->bcType       = static_cast< BCType_t >( bcTypeMap->OneFlow2Cgns( bcRegion->bcType ) );
            cgnsBcRegion->pointSetType = PointRange;
            cgnsBcRegion->CreateCgnsBcConn();
            cgnsBcRegion->connList[ 0 ] = eIdPos + 1;
            cgnsBcRegion->connList[ 1 ] = eIdPos + nRegionCell;
            string bcName = GetCgnsBcName( cgnsBcRegion->bcType );
            cgnsBcRegion->name = AddString( bcName, ir );

            eIdPos += nRegionCell;

            ONEFLOW::SetUnsBcConn( bcRegion, bConnList, pos, unsId );

            irc ++;
        }
    }

    delete bcTypeMap;
}

void ComputeUnsId( StrGrid * grid, PointSearch * pointSearch, Int3D * unsId )
{
    int ni = grid->ni;
    int nj = grid->nj;
    int nk = grid->nk;

    Field3D & xs = * grid->strx;
    Field3D & ys = * grid->stry;
    Field3D & zs = * grid->strz;

    RealField coordinate( 3 );
    for ( int k = 1; k <= nk; ++ k )
    {
        for ( int j = 1; j <= nj; ++ j )
        {
            for ( int i = 1; i <= ni; ++ i )
            {
                Real xm = xs( i, j, k );
                Real ym = ys( i, j, k );
                Real zm = zs( i, j, k );

                int pointIndex = pointSearch->AddPoint( xm, ym, zm );

                //{
                //    int width = 8;
                //    cout << " id = " << pointIndex;
                //    cout << setw( width ) << xm;
                //    cout << setw( width ) << ym;
                //    cout << setw( width ) << zm;
                //    cout << "\n";
                //}
                

                ( * unsId )( i, j, k ) = pointIndex;
            }
        }
    }
}

void SetUnsBcConn( BcRegion * bcRegion, CgIntField& conn, int & pos, Int3D & unsId )
{
    int ist, ied, jst, jed, kst, ked;
    bcRegion->GetNormalizeIJKRegion( ist, ied, jst, jed, kst, ked );

    cout << " ist, ied, jst, jed, kst, ked = " << ist << " " << ied << " " << jst << " " << jed << " " << kst << " " << ked << endl;
    int numpt = 4;
    if ( Dim::dimension == TWO_D ) numpt = 2;

    if ( ist == ied )
    {
        int i = ist;
        if ( Dim::dimension == THREE_D )
        {
            for ( int k = kst; k <= ked - 1; ++ k )
            {
                for ( int j = jst; j <= jed - 1; ++ j )
                {
                    if ( i == 1 )
                    {
                        conn[ pos + 0 ] = unsId( i, j    , k     ) + 1;
                        conn[ pos + 1 ] = unsId( i, j    , k + 1 ) + 1;
                        conn[ pos + 2 ] = unsId( i, j + 1, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i, j + 1, k     ) + 1;
                    }
                    else
                    {
                        conn[ pos + 0 ] = unsId( i, j    , k     ) + 1;
                        conn[ pos + 1 ] = unsId( i, j + 1, k     ) + 1;
                        conn[ pos + 2 ] = unsId( i, j + 1, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i, j    , k + 1 ) + 1;
                    }
                    pos += numpt;
                }
            }
        }
        else
        {
            int k = kst;
            for ( int j = jst; j <= jed - 1; ++ j )
            {
                if ( i == 1 )
                {
                    conn[ pos + 0 ] = unsId( i, j + 1, k  ) + 1;
                    conn[ pos + 1 ] = unsId( i, j    , k  ) + 1;
                }
                else
                {
                    conn[ pos + 0 ] = unsId( i, j    , k  ) + 1;
                    conn[ pos + 1 ] = unsId( i, j + 1, k  ) + 1;
                }
                pos += numpt;
            }
        }
        return;
    }

    if ( jst == jed )
    {
        int j = jst;
        if ( Dim::dimension == THREE_D )
        {
            for ( int k = kst; k <= ked - 1; ++ k )
            {
                for ( int i = ist; i <= ied - 1; ++ i )
                {
                    if ( j == 1 )
                    {
                        conn[ pos + 0 ] = unsId( i    , j, k    ) + 1;
                        conn[ pos + 1 ] = unsId( i + 1, j, k    ) + 1;
                        conn[ pos + 2 ] = unsId( i + 1, j, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i    , j, k + 1 ) + 1;
                    }   
                    else
                    {
                        conn[ pos + 0 ] = unsId( i    , j, k     ) + 1;
                        conn[ pos + 1 ] = unsId( i    , j, k + 1 ) + 1;
                        conn[ pos + 2 ] = unsId( i + 1, j, k + 1 ) + 1;
                        conn[ pos + 3 ] = unsId( i + 1, j, k     ) + 1;
                    }
                    pos += numpt;
                }
            }
        }
        else
        {
            int k = kst;
            for ( int i = ist; i <= ied - 1; ++ i )
            {
                if ( j == 1 )
                {
                    conn[ pos + 0 ] = unsId( i    , j, k  ) + 1;
                    conn[ pos + 1 ] = unsId( i + 1, j, k  ) + 1;
                }   
                else
                {
                    conn[ pos + 0 ] = unsId( i + 1, j, k  ) + 1;
                    conn[ pos + 1 ] = unsId( i    , j, k  ) + 1;
                }
                pos += numpt;
            }
        }
        return;
    }

    if ( kst == ked )
    {
        int k = kst;
        for ( int j = jst; j <= jed - 1; ++ j )
        {
            for ( int i = ist; i <= ied - 1; ++ i )
            {
                if ( k == 1 )
                {
                    conn[ pos + 0 ] = unsId( i    , j    , k ) + 1;
                    conn[ pos + 1 ] = unsId( i    , j + 1, k ) + 1;
                    conn[ pos + 2 ] = unsId( i + 1, j + 1, k ) + 1;
                    conn[ pos + 3 ] = unsId( i + 1, j    , k ) + 1;
                }   
                else
                {
                    conn[ pos + 0 ] = unsId( i    , j    , k ) + 1;
                    conn[ pos + 1 ] = unsId( i + 1, j    , k ) + 1;
                    conn[ pos + 2 ] = unsId( i + 1, j + 1, k ) + 1;
                    conn[ pos + 3 ] = unsId( i    , j + 1, k ) + 1;
                }
                pos += numpt;
            }
        }
        return;
    }

    Stop( " error : ist != ied, jst != jed, kst != ked \n" );
}


#endif
EndNameSpace