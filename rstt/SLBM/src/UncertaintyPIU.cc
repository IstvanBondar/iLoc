//- ****************************************************************************
//-
//- Copyright 2009 National Technology & Engineering Solutions of Sandia, LLC
//- (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
//- Government retains certain rights in this software.
//-
//- BSD Open Source License
//- All rights reserved.
//-
//- Redistribution and use in source and binary forms, with or without
//- modification, are permitted provided that the following conditions are met:
//-
//-   1. Redistributions of source code must retain the above copyright notice,
//-      this list of conditions and the following disclaimer.
//-
//-   2. Redistributions in binary form must reproduce the above copyright
//-      notice, this list of conditions and the following disclaimer in the
//-      documentation and/or other materials provided with the distribution.
//-
//-   3. Neither the name of the copyright holder nor the names of its
//-      contributors may be used to endorse or promote products derived from
//-      this software without specific prior written permission.
//-
//- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//- POSSIBILITY OF SUCH DAMAGE.
//-
//- ****************************************************************************

// **** _SYSTEM INCLUDES_ ******************************************************
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <iomanip>

//using namespace std;

#include "UncertaintyPIU.h"
#include "SLBMException.h"
#include "IFStreamAscii.h"
#include "CPPUtils.h"

using namespace geotess;

// **** _BEGIN SLBM NAMESPACE_ **************************************************

namespace slbm {

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Default Constructor
//
// *****************************************************************************
UncertaintyPIU::UncertaintyPIU() : fname("not_specified"), phaseNum(-1), attributeNum(-1)
{
}  // END Uncertainty Default Constructor


UncertaintyPIU::UncertaintyPIU(string modelPath, const string& phase, int phasenum)
: fname("not_specified"), phaseNum(phasenum), attributeNum(UncertaintyPIU::getAttribute(phase))
{
    fname = "Uncertainty_" + phase + "_" +
            getAttribute(attributeNum) + ".txt";

    fname = CPPUtils::insertPathSeparator(modelPath, fname);

    readFile(fname);
}
// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Parameterized Uncertainty Constructor used by SLBM.  attribute is one of
// "TT", "Sh", "Az"
//
// *****************************************************************************
UncertaintyPIU::UncertaintyPIU(const int& phase, const int& attribute)
: fname("not_specified"), phaseNum(phase), attributeNum(attribute)
{
}

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Parameterized Uncertainty Constructor used by SLBM.  attribute is one of
// "TT", "Sh", "Az"
//
// *****************************************************************************
UncertaintyPIU::UncertaintyPIU(const string& phase, const string& attribute)
: fname("not_specified"), phaseNum(UncertaintyPIU::getPhase(phase)), attributeNum(UncertaintyPIU::getAttribute(phase))
{
}

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Parameterized Uncertainty Constructor used by SLBM.  attribute is one of
// "TT", "Sh", "Az"
//
// *****************************************************************************
UncertaintyPIU::UncertaintyPIU(string modelPath, const int& phase, const int& attribute)
: fname("not_specified"), phaseNum(phase), attributeNum(attribute)
{
    fname = "Uncertainty_" + getPhase(phaseNum) + "_" +
            getAttribute(attributeNum) + ".txt";

    fname = CPPUtils::insertPathSeparator(modelPath, fname);

    readFile(fname);
}

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Copy constructor.
//
// *****************************************************************************
UncertaintyPIU::UncertaintyPIU(const UncertaintyPIU& u) :
                                fname(u.fname),
                                phaseNum(u.phaseNum),
                                attributeNum(u.attributeNum),
                                errDistances(u.errDistances),
                                errDepths(u.errDepths),
                                errVal(u.errVal)
{
}

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Uncertainty Constructor that builds the object from the input DataBuffer.
//
// *****************************************************************************
UncertaintyPIU::UncertaintyPIU(util::DataBuffer& buffer)
{
    deserialize(buffer);
}

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Uncertainty Destructor
//
// *****************************************************************************
UncertaintyPIU::~UncertaintyPIU()
{
    fname = "not_specified";
    phaseNum = attributeNum = -1;
    errDistances.clear();
    errDepths.clear();
    errVal.clear();
}

// **** _FUNCTION DESCRIPTION_ *************************************************
//
// Assignment operator.
//
// *****************************************************************************
UncertaintyPIU& UncertaintyPIU::operator=(const UncertaintyPIU& u)
{
    phaseNum     = u.phaseNum;
    attributeNum = u.attributeNum;
    errDepths = u.errDepths;
    errDistances = u.errDistances;
    errVal       = u.errVal;

    return *this;
}

bool UncertaintyPIU::operator==(const UncertaintyPIU& other) const
{
    if (this->attributeNum != other.attributeNum) return false;
    if (this->phaseNum != other.phaseNum) return false;
    if (this->errDepths.size() != other.errDepths.size()) return false;
    if (this->errDistances.size() != other.errDistances.size()) return false;
    if (this->errVal.size() != other.errVal.size() ) return false;

    for (int i=0; i<(int)errDistances.size(); ++i)
        if (abs(this->errDistances[i]/other.errDistances[i]-1.) > 1e-6)
            return false;

    for (int i=0; i<(int)errDepths.size(); ++i)
        if (abs(this->errDepths[i]/(float)other.errDepths[i]-1.) > 1e-6)
            return false;

    for (int i=0; i<(int)errVal.size(); ++i)
    {
        if (this->errVal[i].size() != other.errVal[i].size())
            return false;
        for (int j=0; j<(int)errVal[i].size(); ++j)
            if (abs(this->errVal[i][j]/other.errVal[i][j]-1.) > 1e-6)
            {
                cout<< "UncertaintyPIU::operator== "
                        << this->errVal[i][j] << " " << other.errVal[i][j] << endl;
                return false;
            }
    }
    return true;
                                }

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(const string& modelPath, const int& phase, const int& attribute)
{
    string fname = "Uncertainty_" +
            getPhase(phase) + "_" +
            getAttribute(attribute) +
            ".txt";

    fname = CPPUtils::insertPathSeparator(modelPath, fname);

    ifstream fin;
    fin.open(fname.c_str());
    if (fin.fail() || !fin.is_open())
        return NULL;

    UncertaintyPIU* u = getUncertaintyPIU(fin, phase, attribute);

    fin.close();

    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(ifstream& input, const int& phase, const int& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->readFile(input);
    if (u->getDistances().size() == 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(ifstream& input, const string& phase, const string& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->readFile(input);
    if (u->getDistances().size() == 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(geotess::IFStreamAscii& input, const int& phase, const int& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->readFile(input);
    if (u->getDistances().size() == 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(geotess::IFStreamAscii& input, const string& phase, const string& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->readFile(input);
    if (u->getDistances().size() == 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(geotess::IFStreamBinary& input, const int& phase, const int& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->readFile(input);
    if (u->getDistances().size() == 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(geotess::IFStreamBinary& input, const string& phase, const string& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->readFile(input);
    if (u->getDistances().size() == 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(util::DataBuffer& buffer, const int& phase, const int& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->deserialize(buffer);
    if (u->phaseNum < 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

UncertaintyPIU* UncertaintyPIU::getUncertaintyPIU(util::DataBuffer& buffer, const string& phase, const string& attribute)
{
    UncertaintyPIU* u = new UncertaintyPIU(phase, attribute);
    u->deserialize(buffer);
    if (u->phaseNum < 0)
    {
        delete u;
        u = NULL;
    }
    return u;
}

void UncertaintyPIU::readFile(const string& filename)
{
    ifstream fin;

    //open the file.  Return an error if cannot open the file.
    fin.open(filename.c_str());
    if (fin.fail() || !fin.is_open())
    {
        ostringstream os;
        os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                <<"Could not open file " << filename << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),115);
    }

    readFile(fin);

    fin.close();
}

void UncertaintyPIU::readFile(ifstream& fin)
{
    errDepths.clear();
    errDistances.clear();
    errVal.clear();

    try
    {
        int numdepths = 0;
        int numdistances;

        fin >> numdistances;
        fin >> numdepths; // this was added for cases where numdepths > 1 and for ascii consistency.

        if (numdistances > 0)
        {
            errDistances.resize(numdistances);
            for( int i = 0; i < numdistances; i++ )
                fin >> errDistances[i];

            // The number of depths can be zero and still have uncertainty values as
            // a function of distance.

            if (numdepths > 0)
            {
                errDepths.resize(numdepths);
                for( int i = 0; i < numdepths; i++ )
                    fin >> errDepths[i];
            }

            // if attribute is 2:AZ, convert degrees to radians, if 1:SH convert sec/deg
            // to sec/radian, if 0:TT no conversion.
            double convert = attributeNum == 2 ? DEG_TO_RAD : attributeNum == 1 ? RAD_TO_DEG : 1.;

            if (numdepths == 0) numdepths = 1;

            string comment;
            errVal.resize(numdepths);
            for (int j = 0; j < numdepths; ++j)
            {
                fin >> comment; // ignore comment
                errVal[j].resize(numdistances);
                for( int i = 0; i < numdistances; i++ )
                    fin >> errVal[j][i];

                if (attributeNum > 0)
                    for( int i = 0; i < numdistances; i++ )
                        errVal[j][i] *= convert;
            }
        }

    }
    catch( ... )
    {
        ostringstream os;
        os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                <<"Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),115);
    }
}

void UncertaintyPIU::readFile(geotess::IFStreamAscii& input)
{
    // The number of depths can be zero and still have uncertainty values as
    // a function of distance.
    try
    {
        errDepths.clear();
        errDistances.clear();
        errVal.clear();

        string line;
        int numdistances = input.readInteger();
        int numdepths = input.readInteger();

        if (numdistances > 0)
        {
            errDistances.reserve(numdistances);
            for( int i = 0; i < numdistances; i++ )
                errDistances.push_back(input.readDouble());

            if (numdepths > 0)
            {
                errDepths.reserve(numdepths);
                for( int i = 0; i < numdepths; i++ )
                    errDepths.push_back(input.readDouble());
            }

            // if attribute is 2:AZ, convert degrees to radians, if 1:SH convert sec/deg
            // to sec/radian, if 0:TT no conversion.
            double convert = attributeNum == 2 ? DEG_TO_RAD : attributeNum == 1 ? RAD_TO_DEG : 1.;

            if (numdepths == 0) numdepths = 1;

            errVal.resize(numdepths);
            for (int j = 0; j < numdepths; ++j)
            {
                input.getLine(line); // ignore comment
                for (int j = 0; j < numdepths; ++j)
                {
                    errVal[j].reserve(numdistances);
                    for( int i = 0; i < numdistances; i++ )
                        errVal[j].push_back(input.readDouble()*convert);
                }
            }
        }
    }
    catch( ... )
    {
        ostringstream os;
        os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                <<"Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),115);
    }
}

void UncertaintyPIU::writeFile(geotess::IFStreamAscii& output)
{
    int nlCount = 8;
    int nDistances = errDistances.size();
    int nDepths = errVal.size() > 1 ? errVal.size() : 0;

    ostringstream os;

    os << nDistances << " " << nDepths << endl;

    os << fixed << setprecision(4) << setw(7);

    for (int i = 0; i < nDistances; i++)
    {
        os << errDistances[i];
        if (((i + 1) % nlCount == 0) ||
                (i == errDistances.size() - 1))
            os << endl;
        else
            os << " ";
    }

    for (int i = 0; i < nDepths; i++)
    {
        os << errDepths[i];
        if (((i + 1) % nlCount == 0) ||
                (i == errDepths.size() - 1))
            os << endl;
        else
            os << " ";
    }

    // if attribute is 2:AZ, convert degrees to radians, if 1:SH convert sec/deg
    // to sec/radian, if 0:TT no conversion.
    double convert = attributeNum == 2 ? DEG_TO_RAD : attributeNum == 1 ? RAD_TO_DEG : 1.;

    for (int i = 0; i < errVal.size(); i++)
    {
        os << "#" << endl;
        for (int j=0; j < nDistances; ++j)
            os << errVal[i][j]/convert << endl;
    }
}

void UncertaintyPIU::readFile(geotess::IFStreamBinary& input)
{
    // The number of depths can be zero and still have uncertainty values as
    // a function of distance.
    try
    {
        errDepths.clear();
        errDistances.clear();
        errVal.clear();

        int numdistances = input.readInt();

        int numdepths = input.readInt();

        if (numdistances > 0)
        {
            errDistances.reserve(numdistances);
            for( int i = 0; i < numdistances; i++ )
                errDistances.push_back(input.readDouble());

            if (numdepths > 0)
            {
                errDepths.reserve(numdepths);
                for( int i = 0; i < numdepths; i++ )
                    errDepths.push_back(input.readDouble());
            }

            // if attribute is 2:AZ, convert degrees to radians, if 1:SH convert sec/deg
            // to sec/radian, if 0:TT no conversion.
            double convert = attributeNum == 2 ? DEG_TO_RAD : attributeNum == 1 ? RAD_TO_DEG : 1.;

            if (numdepths == 0) numdepths = 1;

            errVal.resize(numdepths);
            for (int j = 0; j < numdepths; ++j)
            {
                errVal[j].reserve(numdistances);
                for( int i = 0; i < numdistances; i++ )
                    errVal[j].push_back(input.readDouble()*convert);
            }
        }
    }
    catch( ... )
    {
        ostringstream os;
        os << endl << "ERROR in UncertaintyPIU::readFile" << endl
                <<"Invalid or corrupt file format" << endl
                << "Version " << SlbmVersion << "  File " << __FILE__ << " line " << __LINE__ << endl << endl;
        throw SLBMException(os.str(),115);
    }
}

void UncertaintyPIU::writeFile(geotess::IFStreamBinary& output)
{
    // if attribute is 2:AZ, convert degrees to radians, if 1:SH convert sec/deg
    // to sec/radian, if 0:TT no conversion.
    double convert = attributeNum == 2 ? DEG_TO_RAD : attributeNum == 1 ? RAD_TO_DEG : 1.;

    // The number of depths can be zero and still have uncertainty values as
    // a function of distance.

    output.writeInt((int)errDistances.size());
    output.writeInt((int)errDepths.size());
    for (int i=0; i<(int)errDistances.size(); ++i)
        output.writeDouble(errDistances[i]);
    for (int j=0; j<(int)errDepths.size(); ++j)
        output.writeDouble(errDepths[j]);
    for (int i=0; i<(int)errVal.size(); ++i)
        for (int j=0; j<(int)errDistances.size(); ++j)
            output.writeDouble(errVal[i][j]/convert);
}

double UncertaintyPIU::getUncertainty(const double& distance, double depth)
{
    // Convert to degrees since model errors are defined in degrees.
    double distanceDeg = distance * RAD_TO_DEG;

    // initialize indices and interpolation weights
    int idist, idepth;
    idist = idepth = 0;
    double wdist, wdepth;
    wdist = wdepth = 0.0;

    if ((errVal.size() == 1) || (depth >= errDepths.back()))
    {
        // if more than one depth and depth exceeds last entry set idepth
        if (errVal.size() > 1) idepth = (int) errDepths.size() - 1;

        // if distance in question is greater than the max defined distance, return
        // uncertainty defined for max distance.  Otherwise, interpolate between the
        // two bracketing distances.
        if( distanceDeg >= errDistances.back())
            return errVal[idepth].back();
        else
        {
            // get distance interpolation index and weight
            getIndex(distanceDeg, errDistances, idist, wdist);
            return getUncertainty(wdist, idist, idepth);
        }
    }
    else
    {
        // get depth interpolation index and weight
        getIndex(depth, errDepths, idepth, wdepth);

        // if distance in question is greater than the max defined distance, return
        // uncertainty defined for max distance.  Otherwise, interpolate between the
        // two bracketing distances.
        if( distanceDeg >= errDistances.back())
            return wdepth * (errVal[idepth+1].back() - errVal[idepth].back()) +
                    errVal[idepth].back();
        else
        {
            // get distance interpolation index and weight
            getIndex(distanceDeg, errDistances, idist, wdist);
            return wdepth * (getUncertainty(wdist, idist, idepth+1) -
                    getUncertainty(wdist, idist, idepth)) +
                    getUncertainty(wdist, idist, idepth);
        }
    }
}

double UncertaintyPIU::getVariance(const double& distance, double depth)
{
    // Convert to degrees since model errors are defined in degrees.
    double distanceDeg = distance * RAD_TO_DEG;

    // initialize indices and interpolation weights
    int idist, idepth;
    idist = idepth = 0;
    double wdist, wdepth;
    wdist = wdepth = 0.0;

    if ((errVal.size() == 1) || (depth >= errDepths.back()))
    {
        // if more than one depth and depth exceeds last entry set idepth
        if (errVal.size() > 1) idepth = (int) errDepths.size() - 1;

        // if distance in question is greater than the max defined distance, return
        // uncertainty defined for max distance.  Otherwise, interpolate between the
        // two bracketing distances.
        if( distanceDeg >= errDistances.back())
            return errVal[idepth].back();
        else
        {
            // get distance interpolation index and weight
            getIndex(distanceDeg, errDistances, idist, wdist);
            return getVariance(wdist, idist, idepth);
        }
    }
    else
    {
        // get depth interpolation index and weight
        getIndex(depth, errDepths, idepth, wdepth);

        // if distance in question is greater than the max defined distance, return
        // uncertainty defined for max distance.  Otherwise, interpolate between the
        // two bracketing distances.
        if( distanceDeg >= errDistances.back())
            return wdepth * (errVal[idepth+1].back() - errVal[idepth].back()) +
                    errVal[idepth].back();
        else
        {
            // get distance interpolation index and weight
            getIndex(distanceDeg, errDistances, idist, wdist);
            return wdepth * (getVariance(wdist, idist, idepth+1) -
                    getVariance(wdist, idist, idepth)) +
                    getVariance(wdist, idist, idepth);
        }
    }
}

void UncertaintyPIU::getIndex(double x, const vector<double>& v,
        int& index, double& w)
{
    // if v consists of only 2 entries then set index to 0 ... otherwise,
    // find bracketing index value
    if (v.size() == 2)
        index = 0;
    else
    {
        // set up interpolation increment and index start
        int inc = (int) v.size();
        inc >>= 1;
        index = inc;

        // perform binary search to find index.
        do
        {
            if (inc > 1) inc >>= 1;
            if (v[index + 1] <= x)
                index += inc;
            else if (v[index] > x)
                index -= inc;
            else
                break;
        } while (true);
    }

    // calculate weight fraction
    w = (x - v[index]) / (v[index+1] - v[index]);
}

string UncertaintyPIU::toStringTable()
{
    ostringstream os;
    os << "Uncertainty Table for " << getPhase(phaseNum) << " / " << getAttribute(attributeNum) << endl;
    os << "        ";
    os << fixed << setprecision(3);
    for (int i=0; i<(int)errDistances.size(); ++i)
        os << " " << setw(7) << errDistances[i];
    os << endl;

    double convert = attributeNum == 2 ? RAD_TO_DEG : attributeNum == 1 ? DEG_TO_RAD : 1.;

    for (int i=0; i<(int)errVal.size(); ++i)
    {
        os << fixed << setprecision(3);
        os << " " << setw(7) << (errDepths.size() == 0 ? 0. : errDepths[i]);
        os << fixed << setprecision(attributeNum == 1 ? 4 : 3);
        for (int j=0; j<(int)errDistances.size(); ++j)
            os << " " << setw(7) << errVal[i][j]*convert;
        os << endl;
    }
    os << endl;
    return os.str();
}

string UncertaintyPIU::toStringFile()
{
    ostringstream os;
    os << fixed << setprecision(3);
    os << errDistances.size();
    if (errDepths.size() > 1)
        os << " " << errVal.size();
    else
        os << " 0";
    os << endl;
    for (int i=0; i<(int)errDistances.size(); ++i)
    {
        os << " " << setw(7) << errDistances[i];
        if ( i%8 == 7 || i == (int)errDistances.size()-1)
            os << endl;
    }
    if (errDepths.size() > 1)
        for (int i=0; i<(int)errDepths.size(); ++i)
        {
            os << " " << setw(7) << errDepths[i];
            if ( i%8 == 7 || i == (int)errDepths.size()-1)
                os << endl;
        }

    double convert = attributeNum == 2 ? RAD_TO_DEG : attributeNum == 1 ? DEG_TO_RAD : 1.;
    os << fixed << setprecision(4);

    for (int i=0; i<(int)errVal.size(); ++i)
    {
        os << "#" << endl;
        for (int j=0; j<(int)errDistances.size(); ++j)
            os << " " << setw(7) << errVal[i][j]*convert << endl;
    }
    return os.str();
}

void UncertaintyPIU::writeFile(const string& directoryName)
{
    string fname = "Uncertainty_" + getPhase(phaseNum) + "_" +
            getAttribute(attributeNum) + ".txt";

    string filename = CPPUtils::insertPathSeparator(directoryName, fname);

    ofstream os(filename.c_str());

    os << fixed << setprecision(3);
    os << errDistances.size();
    if (errDepths.size() > 1)
        os << " " << errVal.size();
    else
        os << " 0";
    os << endl;

    for (int i=0; i<(int)errDistances.size(); ++i)
    {
        os << " " << setw(7) << errDistances[i];
        if ( i%8 == 7 || i == (int)errDistances.size()-1)
            os << endl;
    }
    if (errDepths.size() > 1)
        for (int i=0; i<(int)errDepths.size(); ++i)
        {
            os << " " << setw(7) << errDepths[i];
            if ( i%8 == 7 || i == (int)errDepths.size()-1)
                os << endl;
        }

    double convert = attributeNum == 2 ? RAD_TO_DEG : attributeNum == 1 ? DEG_TO_RAD : 1.;
    os << fixed << setprecision(attributeNum == 1 ? 4 : 3);

    for (int i=0; i<(int)errVal.size(); ++i)
    {
        os << "#" << endl;
        for (int j=0; j<(int)errDistances.size(); ++j)
            os << " " << setw(7) << errVal[i][j]*convert << endl;
    }
    os.close();
}

int UncertaintyPIU::getBufferSize()
{
    int buffsize = sizeof(int);
    buffsize += sizeof(int)+getPhaseStr().size();
    buffsize += sizeof(int)+fname.size();

    buffsize += sizeof(int);
    buffsize += sizeof(int);

    buffsize += errDistances.size() * sizeof(double);
    if (errVal.size() > 1)
        buffsize += errDepths.size() * sizeof(double);

    for (int i = 0; i < (int)errVal.size(); ++i)
        buffsize +=  errDistances.size() * sizeof(double);

    return buffsize;
}

void UncertaintyPIU::serialize(util::DataBuffer& buffer)
{
    // write phaseNum, phaseStr, and fname
    buffer.writeInt32(phaseNum);
    buffer.writeString(getPhaseStr());
    buffer.writeString(fname);

    // write array distance and depth storage size
    buffer.writeInt32((int) errDistances.size());
    if (errVal.size() == 1)
        buffer.writeInt32(0);
    else
        buffer.writeInt32((int) errDepths.size());

    // Write out vector of distances, depths and uncertainties
    buffer.writeDoubleArray(&errDistances[0], errDistances.size());
    if (errVal.size() > 1)
        buffer.writeDoubleArray(&errDepths[0], errDepths.size());

    // Write out all uncertainties
    for (int i = 0; i < (int)errVal.size(); ++i)
        buffer.writeDoubleArray(&errVal[i][0], errDistances.size());
}

void UncertaintyPIU::deserialize(util::DataBuffer& buffer)
{
    int i;

    // read phaseNum, phaseStr, and fname
    phaseNum = buffer.readInt32();

    if (phaseNum < 0)
        return;

    //phaseStr = buffer.readString();
    // ignore the phase string
    buffer.readString();
    fname    = buffer.readString();

    // read distance and depth array storage size
    int ndist = buffer.readInt32();
    int ndpth = buffer.readInt32();

    // resize errDistances and errDepths and read arrays
    errDistances.resize(ndist);
    buffer.readDoubleArray(&errDistances[0], ndist);
    if (ndpth > 0)
    {
        errDepths.resize(ndpth);
        buffer.readDoubleArray(&errDepths[0], ndpth);
    }
    else
    {
        ndpth = 1;
        errDepths.clear();
    }

    // resize errVal and read array
    errVal.resize(ndpth);
    for (i = 0; i < ndpth; ++i)
    {
        errVal[i].resize(ndist);
        buffer.readDoubleArray(&errVal[i][0], ndist);
    }
}

} // end slbm namespace
