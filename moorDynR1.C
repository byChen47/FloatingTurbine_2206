/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2019-2020 OpenCFD Ltd.
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

References
    Chen, H., & Hall, M. (2022). CFD simulation of floating body motion with
    mooring dynamics: Coupling MoorDyn with OpenFOAM. Applied Ocean
    Research, 124, 103210. https://doi.org/10.1016/j.apor.2022.103210
    
    Chen, H., Medina, T. A., & Cercos-Pita, J. L. (2024). CFD simulation of multiple
    moored floating structures using OpenFOAM: An open-access mooring restraints 
    library. Ocean Engineering, 303, 117697.
    https://doi.org/10.1016/j.oceaneng.2024.117697

\*---------------------------------------------------------------------------*/

#include "moorDynR1.H"
#include "addToRunTimeSelectionTable.H"
#include "sixDoFRigidBodyMotion.H"
//#include "Time.H"
#include "fvMesh.H"
#include "OFstream.H"
#include "quaternion.H"

// include MoorDyn header
#include "MoorDynv1.h"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace sixDoFRigidBodyMotionRestraints
{
    defineTypeNameAndDebug(moorDynR1, 0);

    addToRunTimeSelectionTable
    (
        sixDoFRigidBodyMotionRestraint,
        moorDynR1,
        dictionary
    );
}
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::sixDoFRigidBodyMotionRestraints::moorDynR1::moorDynR1
(
    const word& name,
    const dictionary& sDoFRBMRDict
)
:
    sixDoFRigidBodyMotionRestraint(name, sDoFRBMRDict)
{
    read(sDoFRBMRDict);
    initialized_ = false;

    Info << "Create moorDynR1 using MoorDyn v1." << endl;

    outputFile_  = new OFstream("BodyMotion.dat");
    outputFile_2 = new OFstream("BodyVelocity.dat");

}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::sixDoFRigidBodyMotionRestraints::moorDynR1::~moorDynR1()
{
    if (initialized_)
    {
        // Close MoorDyn call
        LinesClose();
    }
}


// * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * * //


void Foam::sixDoFRigidBodyMotionRestraints::moorDynR1::restrain
(
    const sixDoFRigidBodyMotion& motion,
    vector& restraintPosition,
    vector& restraintForce,
    vector& restraintMoment
) const
{
    scalar deltaT = motion.time().deltaTValue();
    scalar t = motion.time().value();
    scalar tprev = t-deltaT;

    point CoM = motion.centreOfMass();
    vector rotationAngle
    (
       quaternion(motion.orientation()).eulerAngles(quaternion::XYZ)
    );

    vector v = motion.v();
    vector omega = motion.omega();

    double X[6], XD[6];
    for (int ii=0; ii<3; ii++)
    {
       X[ii] = CoM[ii];
       X[ii+3] = rotationAngle[ii];
       XD[ii] = v[ii];
       XD[ii+3] = omega[ii];
    }

    if (!initialized_)
    {
        // Initialize MoorDyn
        //LinesInit(&CoM[0], &v[0]);
        LinesInit(X, XD);
        Info<< "MoorDyn module initialized!" << endl;
        initialized_ = true;
    }

    double Flines[6] = {0.0};

    // Call LinesCalc() to obtain forces and moments, Flines(1x6)
    // LinesCalc(double X[], double XD[], double Flines[], double* t_in, double* dt_in)

    Info<< "X[6]: " << vector(X[0], X[1], X[2]) << ", " << vector(X[3], X[4], X[5])
        << endl;

    Info<< "XD[6]: " << vector(XD[0], XD[1], XD[2]) << ", " << vector(XD[3], XD[4], XD[5])
        << endl;

    //LinesCalc(&CoM[0], &v[0], Flines, &t, &deltaT);
    LinesCalc(X, XD, Flines, &tprev, &deltaT);
/*
    Info<< "Mooring [force, moment] = [ "
        << Flines[0] << " " << Flines[1] << " " << Flines[2] << ", "
        << Flines[3] << " " << Flines[4] << " " << Flines[5] << " ]"
        << endl;
*/

    for(int i=0;i<3;i++)
    {
        restraintForce[i]=Flines[i];
        restraintMoment[i]=Flines[i+3];
    }

    // Since moment is already calculated by LinesCalc, set to
    // centreOfRotation to be sure of no spurious moment
    restraintPosition = motion.centreOfRotation();

    if (motion.report())
    {
        Info<< t << ": force " << restraintForce
	    << ", moment " << restraintMoment
            << endl;
    }
    *outputFile_ << t << " " << CoM[0] << " " << CoM[1] << " "<<CoM[2] <<" "
                 << 180/3.1415926535 * rotationAngle[0] <<" " <<180/3.1415926535 * rotationAngle[1] <<" " <<180/3.1415926535 * rotationAngle[2] 
                 << " "<< endl;
    *outputFile_2 << t << " " << v[0] << " " << v[1] << " "<<v[2] <<" "
                 << omega[0] <<" " << omega[1] <<" " << omega[2] 
                 << " "<< endl;              
  //  
}


bool Foam::sixDoFRigidBodyMotionRestraints::moorDynR1::read
(
    const dictionary& sDoFRBMRDict
)
{
    sixDoFRigidBodyMotionRestraint::read(sDoFRBMRDict);

    return true;
}


void Foam::sixDoFRigidBodyMotionRestraints::moorDynR1::write
(
    Ostream& os
) const
{
}


// ************************************************************************* //
