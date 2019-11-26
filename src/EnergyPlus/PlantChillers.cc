// EnergyPlus, Copyright (c) 1996-2019, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>
#include <ObjexxFCL/gio.hh>
#include <ObjexxFCL/string.functions.hh>

// EnergyPlus Headers
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/CurveManager.hh>
#include <EnergyPlus/DataBranchAirLoopPlant.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataIPShortCuts.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataPlant.hh>
#include <EnergyPlus/DataPrecisionGlobals.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/EMSManager.hh>
#include <EnergyPlus/FaultsManager.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/GeneralRoutines.hh>
#include <EnergyPlus/GlobalNames.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutAirNodeManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/OutputReportPredefined.hh>
#include <EnergyPlus/PlantChillers.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ReportSizingManager.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>

namespace EnergyPlus {

namespace PlantChillers {

    // MODULE INFORMATION:
    //       AUTHOR         Dan Fisher / Brandon Anderson
    //       DATE WRITTEN   September 2000
    //       MODIFIED       Richard Liesen Nov-Dec 2001; Jan 2002
    //                      Chandan Sharma, FSEC, February 2010, Added basin heater
    //       RE-ENGINEERED  Edwin: Merged Four Chiller Modules Into One

    // PURPOSE OF THIS MODULE:
    // This module simulates the performance of the Electric vapor
    // compression Chillers, Gas Turbine Chillers, Engine Drivent chillers, and
    // Constant COP chillers

    // METHODOLOGY EMPLOYED:
    // Called by plantloopequipment, model accepts inputs, and calculates a
    // thermal response using new plant routines such as SetComponentFlowRate

    // REFERENCES:
    // 1. BLAST Users Manual

    // Using/Aliasing
    using namespace DataPrecisionGlobals;
    using namespace DataLoopNode;
    using DataGlobals::DisplayExtraWarnings;
    using DataGlobals::MaxNameLength;
    using DataGlobals::NumOfTimeStepInHour;
    using DataGlobals::WarmupFlag;
    using DataHVACGlobals::SmallWaterVolFlow;
    using DataPlant::DeltaTempTol;
    using DataPlant::TypeOf_Chiller_CombTurbine;
    using DataPlant::TypeOf_Chiller_ConstCOP;
    using DataPlant::TypeOf_Chiller_Electric;
    using DataPlant::TypeOf_Chiller_EngineDriven;
    using General::RoundSigDigits;
    using General::TrimSigDigits;

    // Parameters for use in Chillers
    int const AirCooled(1);
    int const WaterCooled(2);
    int const EvapCooled(3);
    Real64 const KJtoJ(1000.0); // convert Kjoules to joules

    // chiller flow modes
    int const FlowModeNotSet(200);
    int const ConstantFlow(201);
    int const NotModulated(202);
    int const LeavingSetPointModulated(203);

    static std::string const BlankString;

    // MODULE VARIABLE DECLARATIONS:
    Real64 modCondMassFlowRate(0.0);    // Kg/s - condenser mass flow rate, water side
    Real64 modEvapMassFlowRate(0.0);    // Kg/s - evaporator mass flow rate, water side
    Real64 modCondOutletTemp(0.0);      // C - condenser outlet temperature, air or water side
    Real64 modCondOutletHumRat(0.0);    // kg/kg - condenser outlet humditiy ratio, air side
    Real64 modEvapOutletTemp(0.0);      // C - evaporator outlet temperature, water side
    Real64 modPower(0.0);               // W - rate of chiller energy use
    Real64 modQEvaporator(0.0);         // W - rate of heat transfer to the evaporator coil
    Real64 modQCondenser(0.0);          // W - rate of heat transfer to the condenser coil
    Real64 modEnergy(0.0);              // J - chiller energy use
    Real64 modEvaporatorEnergy(0.0);    // J - rate of heat transfer to the evaporator coil
    Real64 modCondenserEnergy(0.0);     // J - rate of heat transfer to the condenser coil
    Real64 modQHeatRecovered(0.0);      // W - rate of heat transfer to the Heat Recovery coil
    Real64 modHeatRecOutletTemp(0.0);   // C - Heat Rec outlet temperature, water side
    Real64 modAvgCondSinkTemp(0.0);     // condenser temperature value for use in curves [C]
    Real64 modChillerCyclingRatio(0.0); // Cycling ratio for chiller when load is below MinPLR
    Real64 modBasinHeaterPower(0.0);    // Basin heater power (W)

    // const COP
    int NumElectricChillers(0);      // number of Electric chillers specified in input
    int NumEngineDrivenChillers(0); // number of EngineDriven chillers specified in input
    int NumGTChillers(0); // number of GT chillers specified in input
    int NumConstCOPChillers(0);

    bool GetEngineDrivenInput(true); // then TRUE, calls subroutine to read input file.
    bool GetElectricInput(true);     // then TRUE, calls subroutine to read input file.
    bool GetGasTurbineInput(true);   // then TRUE, calls subroutine to read input file.
    bool GetConstCOPInput(true);

    // Object Data
    Array1D<ElectricChillerSpecs> ElectricChiller; // dimension to number of machines
    Array1D<ElectricReportVars> ElectricChillerReport;
    Array1D<EngineDrivenChillerSpecs> EngineDrivenChiller; // dimension to number of machines
    Array1D<EngineDrivenReportVars> EngineDrivenChillerReport;
    Array1D<GTChillerSpecs> GTChiller; // dimension to number of machines
    Array1D<GasTurbineReportVars> GTChillerReport;
    Array1D<ConstCOPChillerSpecs> ConstCOPChiller; // dimension to number of machines
    Array1D<ConstCOPReportVars> ConstCOPChillerReport;

    void clear_state()
    {
        NumElectricChillers = 0;
        modCondMassFlowRate = 0.0;
        modEvapMassFlowRate = 0.0;
        modCondOutletTemp = 0.0;
        modCondOutletHumRat = 0.0;
        modEvapOutletTemp = 0.0;
        modPower = 0.0;
        modQEvaporator = 0.0;
        modQCondenser = 0.0;
        modEnergy = 0.0;
        modEvaporatorEnergy = 0.0;
        modCondenserEnergy = 0.0;
        modQHeatRecovered = 0.0;
        modHeatRecOutletTemp = 0.0;
        modAvgCondSinkTemp = 0.0;
        modChillerCyclingRatio = 0.0;
        modBasinHeaterPower = 0.0;
        NumEngineDrivenChillers = 0;
        NumGTChillers = 0;
        NumConstCOPChillers = 0;
        GetEngineDrivenInput = true;
        GetElectricInput = true;
        GetGasTurbineInput = true;
        GetConstCOPInput = true;
        ElectricChiller.deallocate();
        ElectricChillerReport.deallocate();
        EngineDrivenChiller.deallocate();
        EngineDrivenChillerReport.deallocate();
        GTChiller.deallocate();
        GTChillerReport.deallocate();
        ConstCOPChiller.deallocate();
        ConstCOPChillerReport.deallocate();
    }

    void SimChiller(int const LoopNum,              // Flow control mode for the equipment
                    int const EP_UNUSED(LoopSide),  // chiller number pointer
                    int const ChillerType,          // type of chiller
                    std::string const &ChillerName, // user specified name of chiller
                    int const EquipFlowCtrl,        // Flow control mode for the equipment
                    int &CompIndex,                 // chiller number pointer
                    bool const RunFlag,             // simulate chiller when TRUE
                    bool const FirstHVACIteration,  // initialize variables when TRUE
                    bool &InitLoopEquip,            // If not zero, calculate the max load for operating conditions
                    Real64 &MyLoad,                 // loop demand component will meet
                    Real64 &MaxCap,                 // W - maximum operating capacity of chiller
                    Real64 &MinCap,                 // W - minimum operating capacity of chiller
                    Real64 &OptCap,                 // W - optimal operating capacity of chiller
                    bool const GetSizingFactor,     // TRUE when just the sizing factor is requested
                    Real64 &SizingFactor,           // sizing factor
                    Real64 &TempCondInDesign,       // design condenser inlet temperature, water side
                    Real64 &TempEvapOutDesign       // design evaporator outlet temperature, water side
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Dan Fisher
        //       DATE WRITTEN   Sept. 1998
        //       MODIFIED       April 1999, May 200-Taecheol Kim
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE: This is the Electric chiller model driver.  It
        // gets the input for the models, initializes simulation variables, call
        // the appropriate model and sets up reporting variables.
        
        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int ChillNum; // chiller number pointer

        {
            auto const SELECT_CASE_var(ChillerType);

            if (SELECT_CASE_var == TypeOf_Chiller_Electric) {

                // Get chiller data from input file
                if (GetElectricInput) {
                    GetElectricChillerInput();
                    GetElectricInput = false;
                }

                // Find the correct Chiller
                if (CompIndex == 0) {
                    ChillNum = UtilityRoutines::FindItemInList(ChillerName, ElectricChiller.ma(&ElectricChillerSpecs::Base));
                    if (ChillNum == 0) {
                        ShowFatalError("SimElectricChiller: Specified Chiller not one of Valid Electric Chillers=" + ChillerName);
                    }
                    CompIndex = ChillNum;
                } else {
                    ChillNum = CompIndex;
                    if (ChillNum > NumElectricChillers || ChillNum < 1) {
                        ShowFatalError("SimElectricChiller:  Invalid CompIndex passed=" + TrimSigDigits(ChillNum) +
                                       ", Number of Units=" + TrimSigDigits(NumElectricChillers) + ", Entered Unit name=" + ChillerName);
                    }
                    if (ElectricChiller(ChillNum).Base.CheckEquipName) {
                        if (ChillerName != ElectricChiller(ChillNum).Base.Name) {
                            ShowFatalError("SimElectricChiller: Invalid CompIndex passed=" + TrimSigDigits(ChillNum) + ", Unit name=" + ChillerName +
                                           ", stored Unit Name for that index=" + ElectricChiller(ChillNum).Base.Name);
                        }
                        ElectricChiller(ChillNum).Base.CheckEquipName = false;
                    }
                }

                if (InitLoopEquip) {
                    TempEvapOutDesign = ElectricChiller(ChillNum).TempDesEvapOut;
                    TempCondInDesign = ElectricChiller(ChillNum).TempDesCondIn;

                    InitElectricChiller(ChillNum, RunFlag, MyLoad);

                    if (LoopNum == ElectricChiller(ChillNum).Base.CWLoopNum) { // chilled water loop
                        SizeElectricChiller(ChillNum);
                        MinCap = ElectricChiller(ChillNum).Base.NomCap * ElectricChiller(ChillNum).MinPartLoadRat;
                        MaxCap = ElectricChiller(ChillNum).Base.NomCap * ElectricChiller(ChillNum).MaxPartLoadRat;
                        OptCap = ElectricChiller(ChillNum).Base.NomCap * ElectricChiller(ChillNum).OptPartLoadRat;
                    } else {
                        MinCap = 0.0;
                        MaxCap = 0.0;
                        OptCap = 0.0;
                    }
                    if (GetSizingFactor) {
                        SizingFactor = ElectricChiller(ChillNum).Base.SizFac;
                    }
                    return;
                }

                // calculate model depending on where called from
                if (LoopNum == ElectricChiller(ChillNum).Base.CWLoopNum) { // chilled water loop

                    InitElectricChiller(ChillNum, RunFlag, MyLoad);
                    CalcElectricChillerModel(ChillNum, MyLoad, EquipFlowCtrl, RunFlag);
                    UpdateElectricChillerRecords(MyLoad, RunFlag, ChillNum);

                } else if (LoopNum == ElectricChiller(ChillNum).Base.CDLoopNum) { // condenser loop
                    PlantUtilities::UpdateChillerComponentCondenserSide(ElectricChiller(ChillNum).Base.CDLoopNum,
                                                        ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                                        TypeOf_Chiller_Electric,
                                                        ElectricChiller(ChillNum).Base.CondInletNodeNum,
                                                        ElectricChiller(ChillNum).Base.CondOutletNodeNum,
                                                        ElectricChillerReport(ChillNum).Base.QCond,
                                                        ElectricChillerReport(ChillNum).Base.CondInletTemp,
                                                        ElectricChillerReport(ChillNum).Base.CondOutletTemp,
                                                        ElectricChillerReport(ChillNum).Base.Condmdot,
                                                        FirstHVACIteration);
                } else if (LoopNum == ElectricChiller(ChillNum).HRLoopNum) { // heat recovery loop
                    PlantUtilities::UpdateComponentHeatRecoverySide(ElectricChiller(ChillNum).HRLoopNum,
                                                    ElectricChiller(ChillNum).HRLoopSideNum,
                                                    TypeOf_Chiller_Electric,
                                                    ElectricChiller(ChillNum).HeatRecInletNodeNum,
                                                    ElectricChiller(ChillNum).HeatRecOutletNodeNum,
                                                    ElectricChillerReport(ChillNum).QHeatRecovery,
                                                    ElectricChillerReport(ChillNum).HeatRecInletTemp,
                                                    ElectricChillerReport(ChillNum).HeatRecOutletTemp,
                                                    ElectricChillerReport(ChillNum).HeatRecMassFlow,
                                                    FirstHVACIteration);
                }

            } else if (SELECT_CASE_var == TypeOf_Chiller_EngineDriven) {

                if (GetEngineDrivenInput) {
                    GetEngineDrivenChillerInput();
                    GetEngineDrivenInput = false;
                }

                // Find the correct Chiller
                if (CompIndex == 0) {
                    ChillNum = UtilityRoutines::FindItemInList(ChillerName, EngineDrivenChiller.ma(&EngineDrivenChillerSpecs::Base));
                    if (ChillNum == 0) {
                        ShowFatalError("SimEngineDrivenChiller: Specified Chiller not one of Valid EngineDriven Chillers=" + ChillerName);
                    }
                    CompIndex = ChillNum;
                } else {
                    ChillNum = CompIndex;
                    if (ChillNum > NumEngineDrivenChillers || ChillNum < 1) {
                        ShowFatalError("SimEngineDrivenChiller:  Invalid CompIndex passed=" + TrimSigDigits(ChillNum) +
                                       ", Number of Units=" + TrimSigDigits(NumEngineDrivenChillers) + ", Entered Unit name=" + ChillerName);
                    }
                    if (EngineDrivenChiller(ChillNum).Base.CheckEquipName) {
                        if (ChillerName != EngineDrivenChiller(ChillNum).Base.Name) {
                            ShowFatalError("SimEngineDrivenChiller: Invalid CompIndex passed=" + TrimSigDigits(ChillNum) + ", Unit name=" +
                                           ChillerName + ", stored Unit Name for that index=" + EngineDrivenChiller(ChillNum).Base.Name);
                        }
                        EngineDrivenChiller(ChillNum).Base.CheckEquipName = false;
                    }
                }

                if (InitLoopEquip) {
                    TempEvapOutDesign = EngineDrivenChiller(ChillNum).TempDesEvapOut;
                    TempCondInDesign = EngineDrivenChiller(ChillNum).TempDesCondIn;

                    InitEngineDrivenChiller(ChillNum, RunFlag, MyLoad);

                    if (LoopNum == EngineDrivenChiller(ChillNum).Base.CWLoopNum) {
                        SizeEngineDrivenChiller(ChillNum);
                        MinCap = EngineDrivenChiller(ChillNum).Base.NomCap * EngineDrivenChiller(ChillNum).MinPartLoadRat;
                        MaxCap = EngineDrivenChiller(ChillNum).Base.NomCap * EngineDrivenChiller(ChillNum).MaxPartLoadRat;
                        OptCap = EngineDrivenChiller(ChillNum).Base.NomCap * EngineDrivenChiller(ChillNum).OptPartLoadRat;
                    } else {
                        MinCap = 0.0;
                        MaxCap = 0.0;
                        OptCap = 0.0;
                    }
                    if (GetSizingFactor) {
                        SizingFactor = EngineDrivenChiller(ChillNum).Base.SizFac;
                    }
                    return;
                }

                // calculate model depending on where called from
                if (LoopNum == EngineDrivenChiller(ChillNum).Base.CWLoopNum) { // chilled water loop
                    InitEngineDrivenChiller(ChillNum, RunFlag, MyLoad);
                    CalcEngineDrivenChillerModel(ChillNum, MyLoad, RunFlag, EquipFlowCtrl);
                    UpdateEngineDrivenChiller(MyLoad, RunFlag, ChillNum);
                } else if (LoopNum == EngineDrivenChiller(ChillNum).Base.CDLoopNum) { // condenser loop
                    PlantUtilities::UpdateChillerComponentCondenserSide(EngineDrivenChiller(ChillNum).Base.CDLoopNum,
                                                        EngineDrivenChiller(ChillNum).Base.CDLoopSideNum,
                                                        TypeOf_Chiller_EngineDriven,
                                                        EngineDrivenChiller(ChillNum).Base.CondInletNodeNum,
                                                        EngineDrivenChiller(ChillNum).Base.CondOutletNodeNum,
                                                        EngineDrivenChillerReport(ChillNum).Base.QCond,
                                                        EngineDrivenChillerReport(ChillNum).Base.CondInletTemp,
                                                        EngineDrivenChillerReport(ChillNum).Base.CondOutletTemp,
                                                        EngineDrivenChillerReport(ChillNum).Base.Condmdot,
                                                        FirstHVACIteration);
                } else if (LoopNum == EngineDrivenChiller(ChillNum).HRLoopNum) { // heat recovery loop
                    PlantUtilities::UpdateComponentHeatRecoverySide(EngineDrivenChiller(ChillNum).HRLoopNum,
                                                    EngineDrivenChiller(ChillNum).HRLoopSideNum,
                                                    TypeOf_Chiller_EngineDriven,
                                                    EngineDrivenChiller(ChillNum).HeatRecInletNodeNum,
                                                    EngineDrivenChiller(ChillNum).HeatRecOutletNodeNum,
                                                    EngineDrivenChillerReport(ChillNum).QTotalHeatRecovered,
                                                    EngineDrivenChillerReport(ChillNum).HeatRecInletTemp,
                                                    EngineDrivenChillerReport(ChillNum).HeatRecOutletTemp,
                                                    EngineDrivenChillerReport(ChillNum).HeatRecMdot,
                                                    FirstHVACIteration);
                }

            } else if (SELECT_CASE_var == TypeOf_Chiller_CombTurbine) {

                if (GetGasTurbineInput) {
                    GetGTChillerInput();
                    GetGasTurbineInput = false;
                }

                if (CompIndex == 0) {
                    ChillNum = UtilityRoutines::FindItemInList(ChillerName, GTChiller.ma(&GTChillerSpecs::Base));
                    if (ChillNum == 0) {
                        ShowFatalError("SimGTChiller: Specified Chiller not one of Valid Gas Turbine Chillers=" + ChillerName);
                    }
                    CompIndex = ChillNum;
                } else {
                    ChillNum = CompIndex;
                    if (ChillNum > NumGTChillers || ChillNum < 1) {
                        ShowFatalError("SimGTChiller:  Invalid CompIndex passed=" + TrimSigDigits(ChillNum) +
                                       ", Number of Units=" + TrimSigDigits(NumGTChillers) + ", Entered Unit name=" + ChillerName);
                    }
                    if (GTChiller(ChillNum).Base.CheckEquipName) {
                        if (ChillerName != GTChiller(ChillNum).Base.Name) {
                            ShowFatalError("SimGTChiller: Invalid CompIndex passed=" + TrimSigDigits(ChillNum) + ", Unit name=" + ChillerName +
                                           ", stored Unit Name for that index=" + GTChiller(ChillNum).Base.Name);
                        }
                        GTChiller(ChillNum).Base.CheckEquipName = false;
                    }
                }

                if (InitLoopEquip) {
                    TempEvapOutDesign = GTChiller(ChillNum).TempDesEvapOut;
                    TempCondInDesign = GTChiller(ChillNum).TempDesCondIn;

                    InitGTChiller(ChillNum, RunFlag, MyLoad);

                    if (LoopNum == GTChiller(ChillNum).Base.CWLoopNum) {
                        SizeGTChiller(ChillNum);
                        MinCap = GTChiller(ChillNum).Base.NomCap * GTChiller(ChillNum).MinPartLoadRat;
                        MaxCap = GTChiller(ChillNum).Base.NomCap * GTChiller(ChillNum).MaxPartLoadRat;
                        OptCap = GTChiller(ChillNum).Base.NomCap * GTChiller(ChillNum).OptPartLoadRat;
                    } else {
                        MinCap = 0.0;
                        MaxCap = 0.0;
                        OptCap = 0.0;
                    }
                    if (GetSizingFactor) {
                        SizingFactor = GTChiller(ChillNum).Base.SizFac;
                    }
                    return;
                }

                // calculate model depending on where called from
                if (LoopNum == GTChiller(ChillNum).Base.CWLoopNum) { // chilled water loop

                    InitGTChiller(ChillNum, RunFlag, MyLoad);
                    CalcGTChillerModel(ChillNum, MyLoad, RunFlag, EquipFlowCtrl);
                    UpdateGTChillerRecords(MyLoad, RunFlag, ChillNum);

                } else if (LoopNum == GTChiller(ChillNum).Base.CDLoopNum) { // condenser loop
                    PlantUtilities::UpdateChillerComponentCondenserSide(GTChiller(ChillNum).Base.CDLoopNum,
                                                        GTChiller(ChillNum).Base.CDLoopSideNum,
                                                        TypeOf_Chiller_CombTurbine,
                                                        GTChiller(ChillNum).Base.CondInletNodeNum,
                                                        GTChiller(ChillNum).Base.CondOutletNodeNum,
                                                        GTChillerReport(ChillNum).Base.QCond,
                                                        GTChillerReport(ChillNum).Base.CondInletTemp,
                                                        GTChillerReport(ChillNum).Base.CondOutletTemp,
                                                        GTChillerReport(ChillNum).Base.Condmdot,
                                                        FirstHVACIteration);
                } else if (LoopNum == GTChiller(ChillNum).HRLoopNum) { // heat recovery loop
                    PlantUtilities::UpdateComponentHeatRecoverySide(GTChiller(ChillNum).HRLoopNum,
                                                    GTChiller(ChillNum).HRLoopSideNum,
                                                    TypeOf_Chiller_CombTurbine,
                                                    GTChiller(ChillNum).HeatRecInletNodeNum,
                                                    GTChiller(ChillNum).HeatRecOutletNodeNum,
                                                    GTChillerReport(ChillNum).HeatRecLubeRate,
                                                    GTChillerReport(ChillNum).HeatRecInletTemp,
                                                    GTChillerReport(ChillNum).HeatRecOutletTemp,
                                                    GTChillerReport(ChillNum).HeatRecMdot,
                                                    FirstHVACIteration);
                }

            } else if (SELECT_CASE_var == TypeOf_Chiller_ConstCOP) {

                // GET INPUT
                if (GetConstCOPInput) {
                    GetConstCOPChillerInput();
                    GetConstCOPInput = false;
                }

                // Find the correct Chiller
                if (CompIndex == 0) {
                    ChillNum = UtilityRoutines::FindItemInList(ChillerName, ConstCOPChiller.ma(&ConstCOPChillerSpecs::Base));
                    if (ChillNum == 0) {
                        ShowFatalError("SimConstCOPChiller: Specified Chiller not one of Valid Constant COP Chillers=" + ChillerName);
                    }
                    CompIndex = ChillNum;
                } else {
                    ChillNum = CompIndex;
                    if (ChillNum > NumConstCOPChillers || ChillNum < 1) {
                        ShowFatalError("SimConstCOPChiller:  Invalid CompIndex passed=" + TrimSigDigits(ChillNum) +
                                       ", Number of Units=" + TrimSigDigits(NumConstCOPChillers) + ", Entered Unit name=" + ChillerName);
                    }
                    if (ConstCOPChiller(ChillNum).Base.CheckEquipName) {
                        if (ChillerName != ConstCOPChiller(ChillNum).Base.Name) {
                            ShowFatalError("SimConstCOPChiller: Invalid CompIndex passed=" + TrimSigDigits(ChillNum) + ", Unit name=" + ChillerName +
                                           ", stored Unit Name for that index=" + ConstCOPChiller(ChillNum).Base.Name);
                        }
                        ConstCOPChiller(ChillNum).Base.CheckEquipName = false;
                    }
                }

                if (InitLoopEquip) {
                    TempEvapOutDesign = 0.0;
                    TempCondInDesign = 0.0;

                    InitConstCOPChiller(ChillNum, RunFlag, MyLoad);

                    if (LoopNum == ConstCOPChiller(ChillNum).Base.CWLoopNum) {
                        SizeConstCOPChiller(ChillNum);
                        MinCap = 0.0;
                        MaxCap = ConstCOPChiller(ChillNum).Base.NomCap;
                        OptCap = ConstCOPChiller(ChillNum).Base.NomCap;
                    } else {
                        MinCap = 0.0;
                        MaxCap = 0.0;
                        OptCap = 0.0;
                    }
                    if (GetSizingFactor) {
                        SizingFactor = ConstCOPChiller(ChillNum).Base.SizFac;
                    }
                    return;
                }

                if (LoopNum == ConstCOPChiller(ChillNum).Base.CWLoopNum) {
                    // Calculate Load
                    // IF MinPlr, MaxPlr, OptPlr are not defined, assume min = 0, max=opt=Nomcap
                    InitConstCOPChiller(ChillNum, RunFlag, MyLoad);
                    CalcConstCOPChillerModel(ChillNum, MyLoad, RunFlag, EquipFlowCtrl);
                    UpdateConstCOPChillerRecords(MyLoad, RunFlag, ChillNum);
                } else if (LoopNum == ConstCOPChiller(ChillNum).Base.CDLoopNum) {
                    PlantUtilities::UpdateChillerComponentCondenserSide(ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                                        ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                                        TypeOf_Chiller_ConstCOP,
                                                        ConstCOPChiller(ChillNum).Base.CondInletNodeNum,
                                                        ConstCOPChiller(ChillNum).Base.CondOutletNodeNum,
                                                        ConstCOPChillerReport(ChillNum).Base.QCond,
                                                        ConstCOPChillerReport(ChillNum).Base.CondInletTemp,
                                                        ConstCOPChillerReport(ChillNum).Base.CondOutletTemp,
                                                        ConstCOPChillerReport(ChillNum).Base.Condmdot,
                                                        FirstHVACIteration);
                }
            }
        }
    }

    void GetElectricChillerInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher / Brandon Anderson
        //       DATE WRITTEN:    September 2000

        // PURPOSE OF THIS SUBROUTINE:
        // This routine will get the input
        // required by the Electric Chiller model.

        // METHODOLOGY EMPLOYED:
        // EnergyPlus input processor

        // Using/Aliasing
        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using BranchNodeConnections::TestCompSet;
        using DataGlobals::AnyEnergyManagementSystemInModel;
        using DataSizing::AutoSize;
        using General::RoundSigDigits;
        using GlobalNames::VerifyUniqueChillerName;
        using NodeInputManager::GetOnlySingleNode;
        using OutAirNodeManager::CheckAndAddAirNodeNumber;
        using PlantUtilities::RegisterPlantCompDesignFlow;
        using ScheduleManager::GetScheduleIndex;

        // Locals
        // PARAMETERS
        static std::string const RoutineName("GetElectricChillerInput: "); // include trailing blank space
        // LOCAL VARIABLES
        int ChillerNum; // chiller counter
        int NumAlphas;  // Number of elements in the alpha array
        int NumNums;    // Number of elements in the numeric array
        int IOStat;     // IO Status when calling get input subroutine
        bool ErrorsFound(false);
        bool Okay;

        // FLOW
        cCurrentModuleObject = "Chiller:Electric";
        NumElectricChillers = inputProcessor->getNumObjectsFound(cCurrentModuleObject);

        if (NumElectricChillers <= 0) {
            ShowSevereError("No " + cCurrentModuleObject + " Equipment specified in input file");
            ErrorsFound = true;
        }

        // See if load distribution manager has already gotten the input
        if (allocated(ElectricChiller)) return;

        // ALLOCATE ARRAYS
        ElectricChiller.allocate(NumElectricChillers);
        ElectricChillerReport.allocate(NumElectricChillers);

        // LOAD ARRAYS WITH Electric CURVE FIT CHILLER DATA
        for (ChillerNum = 1; ChillerNum <= NumElectricChillers; ++ChillerNum) {
            inputProcessor->getObjectItem(cCurrentModuleObject,
                                          ChillerNum,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          lNumericFieldBlanks,
                                          lAlphaFieldBlanks,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(cAlphaArgs(1), cCurrentModuleObject, ErrorsFound);

            // ErrorsFound will be set to True if problem was found, left untouched otherwise
            VerifyUniqueChillerName(cCurrentModuleObject, cAlphaArgs(1), ErrorsFound, cCurrentModuleObject + " Name");

            ElectricChiller(ChillerNum).Base.Name = cAlphaArgs(1);

            if (cAlphaArgs(2) == "AIRCOOLED") {
                ElectricChiller(ChillerNum).Base.CondenserType = AirCooled;
            } else if (cAlphaArgs(2) == "WATERCOOLED") {
                ElectricChiller(ChillerNum).Base.CondenserType = WaterCooled;
            } else if (cAlphaArgs(2) == "EVAPORATIVELYCOOLED") {
                ElectricChiller(ChillerNum).Base.CondenserType = EvapCooled;
            } else {
                ShowSevereError("Invalid " + cAlphaFieldNames(2) + '=' + cAlphaArgs(2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            ElectricChiller(ChillerNum).Base.NomCap = rNumericArgs(1);
            if (ElectricChiller(ChillerNum).Base.NomCap == AutoSize) {
                ElectricChiller(ChillerNum).Base.NomCapWasAutoSized = true;
            }
            if (rNumericArgs(1) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(1) + '=' + RoundSigDigits(rNumericArgs(1), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }
            ElectricChiller(ChillerNum).Base.COP = rNumericArgs(2);
            if (rNumericArgs(2) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(2) + '=' + RoundSigDigits(rNumericArgs(2), 3));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }
            ElectricChiller(ChillerNum).Base.EvapInletNodeNum = GetOnlySingleNode(
                cAlphaArgs(3), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 1, ObjectIsNotParent);
            ElectricChiller(ChillerNum).Base.EvapOutletNodeNum = GetOnlySingleNode(
                cAlphaArgs(4), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 1, ObjectIsNotParent);
            TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(3), cAlphaArgs(4), "Chilled Water Nodes");

            if (ElectricChiller(ChillerNum).Base.CondenserType == AirCooled || ElectricChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                // Connection not required for air or evap cooled condenser
                // If the condenser inlet is blank for air cooled and evap cooled condensers then supply a generic name
                //  since it is not used elsewhere for connection
                // for transition purposes, add this node if not there.
                if (lAlphaFieldBlanks(5)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 21) { // protect against long name leading to > 100 chars
                        cAlphaArgs(5) = cAlphaArgs(1) + " CONDENSER INLET NODE";
                    } else {
                        cAlphaArgs(5) = cAlphaArgs(1).substr(0, 79) + " CONDENSER INLET NODE";
                    }
                }
                if (lAlphaFieldBlanks(6)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 22) { // protect against long name leading to > 100 chars
                        cAlphaArgs(6) = cAlphaArgs(1) + " CONDENSER OUTLET NODE";
                    } else {
                        cAlphaArgs(6) = cAlphaArgs(1).substr(0, 78) + " CONDENSER OUTLET NODE";
                    }
                }

                ElectricChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                      ErrorsFound,
                                                                                      cCurrentModuleObject,
                                                                                      cAlphaArgs(1),
                                                                                      NodeType_Air,
                                                                                      NodeConnectionType_OutsideAirReference,
                                                                                      2,
                                                                                      ObjectIsNotParent);
                CheckAndAddAirNodeNumber(ElectricChiller(ChillerNum).Base.CondInletNodeNum, Okay);
                if (!Okay) {
                    ShowWarningError(cCurrentModuleObject + ", Adding OutdoorAir:Node=" + cAlphaArgs(5));
                }

                ElectricChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(6), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Air, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
            } else if (ElectricChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                ElectricChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(5), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 2, ObjectIsNotParent);
                ElectricChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(6), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(5), cAlphaArgs(6), "Condenser Water Nodes");
                // Condenser Inlet node name is necessary for Water Cooled
                if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(6)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(6) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            } else {
                ElectricChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                      ErrorsFound,
                                                                                      cCurrentModuleObject,
                                                                                      cAlphaArgs(1),
                                                                                      NodeType_Unknown,
                                                                                      NodeConnectionType_Inlet,
                                                                                      2,
                                                                                      ObjectIsNotParent);
                ElectricChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(cAlphaArgs(6),
                                                                                       ErrorsFound,
                                                                                       cCurrentModuleObject,
                                                                                       cAlphaArgs(1),
                                                                                       NodeType_Unknown,
                                                                                       NodeConnectionType_Outlet,
                                                                                       2,
                                                                                       ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(5), cAlphaArgs(6), "Condenser (unknown?) Nodes");
                // Condenser Inlet node name is necessary
                if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(6)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(6) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            }

            ElectricChiller(ChillerNum).MinPartLoadRat = rNumericArgs(3);
            ElectricChiller(ChillerNum).MaxPartLoadRat = rNumericArgs(4);
            ElectricChiller(ChillerNum).OptPartLoadRat = rNumericArgs(5);
            ElectricChiller(ChillerNum).TempDesCondIn = rNumericArgs(6);
            ElectricChiller(ChillerNum).TempRiseCoef = rNumericArgs(7);
            ElectricChiller(ChillerNum).TempDesEvapOut = rNumericArgs(8);
            ElectricChiller(ChillerNum).Base.EvapVolFlowRate = rNumericArgs(9);
            if (ElectricChiller(ChillerNum).Base.EvapVolFlowRate == AutoSize) {
                ElectricChiller(ChillerNum).Base.EvapVolFlowRateWasAutoSized = true;
            }
            ElectricChiller(ChillerNum).Base.CondVolFlowRate = rNumericArgs(10);
            if (ElectricChiller(ChillerNum).Base.CondVolFlowRate == AutoSize) {
                if (ElectricChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                    ElectricChiller(ChillerNum).Base.CondVolFlowRateWasAutoSized = true;
                }
            }
            ElectricChiller(ChillerNum).CapRatCoef(1) = rNumericArgs(11);
            ElectricChiller(ChillerNum).CapRatCoef(2) = rNumericArgs(12);
            ElectricChiller(ChillerNum).CapRatCoef(3) = rNumericArgs(13);
            if ((rNumericArgs(11) + rNumericArgs(12) + rNumericArgs(13)) == 0.0) {
                ShowSevereError(cCurrentModuleObject + ": Sum of Capacity Ratio Coef = 0.0, chiller=" + cAlphaArgs(1));
                ErrorsFound = true;
            }
            ElectricChiller(ChillerNum).PowerRatCoef(1) = rNumericArgs(14);
            ElectricChiller(ChillerNum).PowerRatCoef(2) = rNumericArgs(15);
            ElectricChiller(ChillerNum).PowerRatCoef(3) = rNumericArgs(16);
            ElectricChiller(ChillerNum).FullLoadCoef(1) = rNumericArgs(17);
            ElectricChiller(ChillerNum).FullLoadCoef(2) = rNumericArgs(18);
            ElectricChiller(ChillerNum).FullLoadCoef(3) = rNumericArgs(19);
            ElectricChiller(ChillerNum).TempLowLimitEvapOut = rNumericArgs(20);
            ElectricChiller(ChillerNum).Base.SizFac = rNumericArgs(22);
            if (ElectricChiller(ChillerNum).Base.SizFac <= 0.0) ElectricChiller(ChillerNum).Base.SizFac = 1.0;

            {
                auto const SELECT_CASE_var(cAlphaArgs(7));
                if (SELECT_CASE_var == "CONSTANTFLOW") {
                    ElectricChiller(ChillerNum).Base.FlowMode = ConstantFlow;
                } else if (SELECT_CASE_var == "VARIABLEFLOW") {
                    ElectricChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                    ShowWarningError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                    ShowContinueError("Key choice is now called \"LeavingSetpointModulated\" and the simulation continues");
                } else if (SELECT_CASE_var == "LEAVINGSETPOINTMODULATED") {
                    ElectricChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                } else if (SELECT_CASE_var == "NOTMODULATED") {
                    ElectricChiller(ChillerNum).Base.FlowMode = NotModulated;
                } else {
                    ShowSevereError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                    ShowContinueError("Available choices are ConstantFlow, NotModulated, or LeavingSetpointModulated");
                    ShowContinueError("Flow mode NotModulated is assumed and the simulation continues.");
                    ElectricChiller(ChillerNum).Base.FlowMode = NotModulated;
                }
            }

            // These are the Heat Recovery Inputs
            ElectricChiller(ChillerNum).DesignHeatRecVolFlowRate = rNumericArgs(21);
            if (ElectricChiller(ChillerNum).DesignHeatRecVolFlowRate == AutoSize) {
                ElectricChiller(ChillerNum).DesignHeatRecVolFlowRateWasAutoSized = true;
            }

            if ((ElectricChiller(ChillerNum).DesignHeatRecVolFlowRate > 0.0) || (ElectricChiller(ChillerNum).DesignHeatRecVolFlowRate == AutoSize)) {
                ElectricChiller(ChillerNum).HeatRecActive = true;
                ElectricChiller(ChillerNum).HeatRecInletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(8), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 3, ObjectIsNotParent);
                if (ElectricChiller(ChillerNum).HeatRecInletNodeNum == 0) {
                    ShowSevereError("Invalid " + cAlphaFieldNames(8) + '=' + cAlphaArgs(8));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
                ElectricChiller(ChillerNum).HeatRecOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(9), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 3, ObjectIsNotParent);
                if (ElectricChiller(ChillerNum).HeatRecOutletNodeNum == 0) {
                    ShowSevereError("Invalid " + cAlphaFieldNames(9) + '=' + cAlphaArgs(9));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }

                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(8), cAlphaArgs(9), "Heat Recovery Nodes");
                if (ElectricChiller(ChillerNum).DesignHeatRecVolFlowRate > 0.0) {
                    RegisterPlantCompDesignFlow(ElectricChiller(ChillerNum).HeatRecInletNodeNum,
                                                ElectricChiller(ChillerNum).DesignHeatRecVolFlowRate);
                }
                // Condenser flow rate must be specified for heat reclaim
                if (ElectricChiller(ChillerNum).Base.CondenserType == AirCooled || ElectricChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    if (ElectricChiller(ChillerNum).Base.CondVolFlowRate <= 0.0) {
                        ShowSevereError("Invalid " + cNumericFieldNames(10) + '=' + RoundSigDigits(rNumericArgs(10), 6));
                        ShowSevereError("Condenser fluid flow rate must be specified for Heat Reclaim applications.");
                        ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                        ErrorsFound = true;
                    }
                }

                if (NumNums > 24) {
                    if (!lNumericFieldBlanks(25)) {
                        ElectricChiller(ChillerNum).HeatRecCapacityFraction = rNumericArgs(25);
                    } else {
                        ElectricChiller(ChillerNum).HeatRecCapacityFraction = 1.0;
                    }
                } else {
                    ElectricChiller(ChillerNum).HeatRecCapacityFraction = 1.0;
                }

                if (NumAlphas > 10) {
                    if (!lAlphaFieldBlanks(11)) {
                        ElectricChiller(ChillerNum).HeatRecInletLimitSchedNum = GetScheduleIndex(cAlphaArgs(11));
                        if (ElectricChiller(ChillerNum).HeatRecInletLimitSchedNum == 0) {
                            ShowSevereError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\"");
                            ShowContinueError("Invalid " + cAlphaFieldNames(11) + '=' + cAlphaArgs(11));
                            ErrorsFound = true;
                        }
                    } else {
                        ElectricChiller(ChillerNum).HeatRecInletLimitSchedNum = 0;
                    }
                } else {
                    ElectricChiller(ChillerNum).HeatRecInletLimitSchedNum = 0;
                }

                if (NumAlphas > 11) {
                    if (!lAlphaFieldBlanks(12)) {
                        ElectricChiller(ChillerNum).HeatRecSetPointNodeNum = GetOnlySingleNode(cAlphaArgs(12),
                                                                                               ErrorsFound,
                                                                                               cCurrentModuleObject,
                                                                                               cAlphaArgs(1),
                                                                                               NodeType_Water,
                                                                                               NodeConnectionType_Sensor,
                                                                                               1,
                                                                                               ObjectIsNotParent);
                    } else {
                        ElectricChiller(ChillerNum).HeatRecSetPointNodeNum = 0;
                    }
                } else {
                    ElectricChiller(ChillerNum).HeatRecSetPointNodeNum = 0;
                }

            } else {
                ElectricChiller(ChillerNum).HeatRecActive = false;
                ElectricChiller(ChillerNum).DesignHeatRecMassFlowRate = 0.0;
                ElectricChiller(ChillerNum).HeatRecInletNodeNum = 0;
                ElectricChiller(ChillerNum).HeatRecOutletNodeNum = 0;
                // if heat recovery is not used, don't care about condenser flow rate for air/evap-cooled equip.
                if (ElectricChiller(ChillerNum).Base.CondenserType == AirCooled || ElectricChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    ElectricChiller(ChillerNum).Base.CondVolFlowRate = 0.0011; // set to avoid errors in calc routine
                }
                if ((!lAlphaFieldBlanks(8)) || (!lAlphaFieldBlanks(9))) {
                    ShowWarningError("Since Design Heat Flow Rate = 0.0, Heat Recovery inactive for " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ShowContinueError("However, Node names were specified for Heat Recovery inlet or outlet nodes");
                }
            }
            //   Basin heater power as a function of temperature must be greater than or equal to 0
            ElectricChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff = rNumericArgs(23);
            if (rNumericArgs(23) < 0.0) {
                ShowSevereError(cCurrentModuleObject + ", \"" + ElectricChiller(ChillerNum).Base.Name +
                                "\" TRIM(cNumericFieldNames(23)) must be >= 0");
                ErrorsFound = true;
            }

            ElectricChiller(ChillerNum).Base.BasinHeaterSetPointTemp = rNumericArgs(24);

            if (ElectricChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                if (NumNums < 24) {
                    ElectricChiller(ChillerNum).Base.BasinHeaterSetPointTemp = 2.0;
                }
                if (ElectricChiller(ChillerNum).Base.BasinHeaterSetPointTemp < 2.0) {
                    ShowWarningError(cCurrentModuleObject + ":\"" + ElectricChiller(ChillerNum).Base.Name + "\", " + cNumericFieldNames(24) +
                                     " is less than 2 deg C. Freezing could occur.");
                }
            }

            if (!lAlphaFieldBlanks(10)) {
                ElectricChiller(ChillerNum).Base.BasinHeaterSchedulePtr = GetScheduleIndex(cAlphaArgs(10));
                if (ElectricChiller(ChillerNum).Base.BasinHeaterSchedulePtr == 0) {
                    ShowWarningError(cCurrentModuleObject + ", \"" + ElectricChiller(ChillerNum).Base.Name + "\" TRIM(cAlphaFieldNames(10)) \"" +
                                     cAlphaArgs(10) + "\" was not found. Basin heater operation will not be modeled and the simulation continues");
                }
            }
            if (NumAlphas > 12) {
                ElectricChiller(ChillerNum).EndUseSubcategory = cAlphaArgs(13);
            } else {
                ElectricChiller(ChillerNum).EndUseSubcategory = "General";
            }
        }

        if (ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + cCurrentModuleObject);
        }

        for (ChillerNum = 1; ChillerNum <= NumElectricChillers; ++ChillerNum) {
            SetupOutputVariable("Chiller Electric Power",
                                OutputProcessor::Unit::W,
                                ElectricChillerReport(ChillerNum).Base.Power,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Electric Energy",
                                OutputProcessor::Unit::J,
                                ElectricChillerReport(ChillerNum).Base.Energy,
                                "System",
                                "Sum",
                                ElectricChiller(ChillerNum).Base.Name,
                                _,
                                "ELECTRICITY",
                                "Cooling",
                                ElectricChiller(ChillerNum).EndUseSubcategory,
                                "Plant");

            SetupOutputVariable("Chiller Evaporator Cooling Rate",
                                OutputProcessor::Unit::W,
                                ElectricChillerReport(ChillerNum).Base.QEvap,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Cooling Energy",
                                OutputProcessor::Unit::J,
                                ElectricChillerReport(ChillerNum).Base.EvapEnergy,
                                "System",
                                "Sum",
                                ElectricChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "CHILLERS",
                                _,
                                "Plant");
            SetupOutputVariable("Chiller Evaporator Inlet Temperature",
                                OutputProcessor::Unit::C,
                                ElectricChillerReport(ChillerNum).Base.EvapInletTemp,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Outlet Temperature",
                                OutputProcessor::Unit::C,
                                ElectricChillerReport(ChillerNum).Base.EvapOutletTemp,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                ElectricChillerReport(ChillerNum).Base.Evapmdot,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Condenser Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                ElectricChillerReport(ChillerNum).Base.QCond,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Condenser Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                ElectricChillerReport(ChillerNum).Base.CondEnergy,
                                "System",
                                "Sum",
                                ElectricChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATREJECTION",
                                _,
                                "Plant");
            SetupOutputVariable("Chiller COP",
                                OutputProcessor::Unit::W_W,
                                ElectricChillerReport(ChillerNum).ActualCOP,
                                "System",
                                "Average",
                                ElectricChiller(ChillerNum).Base.Name);

            // Condenser mass flow and outlet temp are valid for water cooled
            if (ElectricChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).Base.CondOutletTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Mass Flow Rate",
                                    OutputProcessor::Unit::kg_s,
                                    ElectricChillerReport(ChillerNum).Base.Condmdot,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
            } else if (ElectricChiller(ChillerNum).Base.CondenserType == AirCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
            } else if (ElectricChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
                if (ElectricChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                    SetupOutputVariable("Chiller Basin Heater Electric Power",
                                        OutputProcessor::Unit::W,
                                        ElectricChillerReport(ChillerNum).Base.BasinHeaterPower,
                                        "System",
                                        "Average",
                                        ElectricChiller(ChillerNum).Base.Name);
                    SetupOutputVariable("Chiller Basin Heater Electric Energy",
                                        OutputProcessor::Unit::J,
                                        ElectricChillerReport(ChillerNum).Base.BasinHeaterConsumption,
                                        "System",
                                        "Sum",
                                        ElectricChiller(ChillerNum).Base.Name,
                                        _,
                                        "Electric",
                                        "CHILLERS",
                                        _,
                                        "Plant");
                }
            }

            // If heat recovery is active then setup report variables
            if (ElectricChiller(ChillerNum).HeatRecActive) {
                SetupOutputVariable("Chiller Total Recovered Heat Rate",
                                    OutputProcessor::Unit::W,
                                    ElectricChillerReport(ChillerNum).QHeatRecovery,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Total Recovered Heat Energy",
                                    OutputProcessor::Unit::J,
                                    ElectricChillerReport(ChillerNum).EnergyHeatRecovery,
                                    "System",
                                    "Sum",
                                    ElectricChiller(ChillerNum).Base.Name,
                                    _,
                                    "ENERGYTRANSFER",
                                    "HEATRECOVERY",
                                    _,
                                    "Plant");
                SetupOutputVariable("Chiller Heat Recovery Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).HeatRecInletTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);

                SetupOutputVariable("Chiller Heat Recovery Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).HeatRecOutletTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);

                SetupOutputVariable("Chiller Heat Recovery Mass Flow Rate",
                                    OutputProcessor::Unit::kg_s,
                                    ElectricChillerReport(ChillerNum).HeatRecMassFlow,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);

                SetupOutputVariable("Chiller Effective Heat Rejection Temperature",
                                    OutputProcessor::Unit::C,
                                    ElectricChillerReport(ChillerNum).ChillerCondAvgTemp,
                                    "System",
                                    "Average",
                                    ElectricChiller(ChillerNum).Base.Name);
            }
            if (AnyEnergyManagementSystemInModel) {
                SetupEMSInternalVariable(
                    "Chiller Nominal Capacity", ElectricChiller(ChillerNum).Base.Name, "[W]", ElectricChiller(ChillerNum).Base.NomCap);
            }
        }
    }

    void GetEngineDrivenChillerInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher / Brandon Anderson
        //       DATE WRITTEN:    September 2000
        // PURPOSE OF THIS SUBROUTINE:
        // This routine will get the input
        // required by the EngineDriven Chiller model.

        // Using/Aliasing
        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using BranchNodeConnections::TestCompSet;
        using CurveManager::GetCurveIndex;
        using DataGlobals::AnyEnergyManagementSystemInModel;
        using DataSizing::AutoSize;
        using General::RoundSigDigits;
        using GlobalNames::VerifyUniqueChillerName;
        using NodeInputManager::GetOnlySingleNode;
        using OutAirNodeManager::CheckAndAddAirNodeNumber;
        using PlantUtilities::RegisterPlantCompDesignFlow;
        using ScheduleManager::GetScheduleIndex;

        // Locals
        // PARAMETERS
        static std::string const RoutineName("GetEngineDrivenChillerInput: "); // include trailing blank space
        // LOCAL VARIABLES
        int ChillerNum; // chiller counter
        int NumAlphas;  // Number of elements in the alpha array
        int NumNums;    // Number of elements in the numeric array
        int IOStat;     // IO Status when calling get input subroutine
        bool ErrorsFound(false);
        bool Okay;

        // FLOW
        cCurrentModuleObject = "Chiller:EngineDriven";
        NumEngineDrivenChillers = inputProcessor->getNumObjectsFound(cCurrentModuleObject);

        if (NumEngineDrivenChillers <= 0) {
            ShowSevereError("No " + cCurrentModuleObject + " equipment specified in input file");
            ErrorsFound = true;
        }
        // See if load distribution manager has already gotten the input
        if (allocated(EngineDrivenChiller)) return;

        // ALLOCATE ARRAYS
        EngineDrivenChiller.allocate(NumEngineDrivenChillers);
        EngineDrivenChillerReport.allocate(NumEngineDrivenChillers);

        // LOAD ARRAYS WITH EngineDriven CURVE FIT CHILLER DATA
        for (ChillerNum = 1; ChillerNum <= NumEngineDrivenChillers; ++ChillerNum) {
            inputProcessor->getObjectItem(cCurrentModuleObject,
                                          ChillerNum,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          lNumericFieldBlanks,
                                          lAlphaFieldBlanks,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(cAlphaArgs(1), cCurrentModuleObject, ErrorsFound);

            // ErrorsFound will be set to True if problem was found, left untouched otherwise 
            VerifyUniqueChillerName(cCurrentModuleObject, cAlphaArgs(1), ErrorsFound, cCurrentModuleObject + " Name");
            
            EngineDrivenChiller(ChillerNum).Base.Name = cAlphaArgs(1);

            EngineDrivenChiller(ChillerNum).Base.NomCap = rNumericArgs(1);
            if (EngineDrivenChiller(ChillerNum).Base.NomCap == AutoSize) {
                EngineDrivenChiller(ChillerNum).Base.NomCapWasAutoSized = true;
            }
            if (rNumericArgs(1) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(1) + '=' + RoundSigDigits(rNumericArgs(1), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).Base.COP = rNumericArgs(2);
            if (rNumericArgs(2) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(2) + '=' + RoundSigDigits(rNumericArgs(2), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            if (cAlphaArgs(2) == "AIRCOOLED") {
                EngineDrivenChiller(ChillerNum).Base.CondenserType = AirCooled;
            } else if (cAlphaArgs(2) == "WATERCOOLED") {
                EngineDrivenChiller(ChillerNum).Base.CondenserType = WaterCooled;
            } else if (cAlphaArgs(2) == "EVAPORATIVELYCOOLED") {
                EngineDrivenChiller(ChillerNum).Base.CondenserType = EvapCooled;
            } else {
                ShowSevereError("Invalid " + cAlphaFieldNames(2) + '=' + cAlphaArgs(2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).Base.EvapInletNodeNum = GetOnlySingleNode(
                cAlphaArgs(3), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 1, ObjectIsNotParent);
            EngineDrivenChiller(ChillerNum).Base.EvapOutletNodeNum = GetOnlySingleNode(
                cAlphaArgs(4), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 1, ObjectIsNotParent);
            TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(3), cAlphaArgs(4), "Chilled Water Nodes");

            if (EngineDrivenChiller(ChillerNum).Base.CondenserType == AirCooled || EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                // Connection not required for air or evap cooled condenser
                // If the condenser inlet is blank for air cooled and evap cooled condensers then supply a generic name
                //  since it is not used elsewhere for connection
                if (lAlphaFieldBlanks(5)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 21) { // protect against long name leading to > 100 chars
                        cAlphaArgs(5) = cAlphaArgs(1) + " CONDENSER INLET NODE";
                    } else {
                        cAlphaArgs(5) = cAlphaArgs(1).substr(0, 79) + " CONDENSER INLET NODE";
                    }
                }
                if (lAlphaFieldBlanks(6)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 22) { // protect against long name leading to > 100 chars
                        cAlphaArgs(6) = cAlphaArgs(1) + " CONDENSER OUTLET NODE";
                    } else {
                        cAlphaArgs(6) = cAlphaArgs(1).substr(0, 78) + " CONDENSER OUTLET NODE";
                    }
                }

                EngineDrivenChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                          ErrorsFound,
                                                                                          cCurrentModuleObject,
                                                                                          cAlphaArgs(1),
                                                                                          NodeType_Air,
                                                                                          NodeConnectionType_OutsideAirReference,
                                                                                          2,
                                                                                          ObjectIsNotParent);
                CheckAndAddAirNodeNumber(EngineDrivenChiller(ChillerNum).Base.CondInletNodeNum, Okay);
                if (!Okay) {
                    ShowWarningError(cCurrentModuleObject + ", Adding OutdoorAir:Node=" + cAlphaArgs(5));
                }

                EngineDrivenChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(6), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Air, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
                // CALL TestCompSet(TRIM(cCurrentModuleObject),cAlphaArgs(1),cAlphaArgs(5),cAlphaArgs(6),'Condenser (Air) Nodes')
            } else if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                EngineDrivenChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(5), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 2, ObjectIsNotParent);
                EngineDrivenChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(6), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(5), cAlphaArgs(6), "Condenser Water Nodes");
                // Condenser Inlet node name is necessary for Water Cooled
                if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + " is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(6)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(6) + " is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            } else {
                EngineDrivenChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                          ErrorsFound,
                                                                                          cCurrentModuleObject,
                                                                                          cAlphaArgs(1),
                                                                                          NodeType_Unknown,
                                                                                          NodeConnectionType_Inlet,
                                                                                          2,
                                                                                          ObjectIsNotParent);
                EngineDrivenChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(cAlphaArgs(6),
                                                                                           ErrorsFound,
                                                                                           cCurrentModuleObject,
                                                                                           cAlphaArgs(1),
                                                                                           NodeType_Unknown,
                                                                                           NodeConnectionType_Outlet,
                                                                                           2,
                                                                                           ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(5), cAlphaArgs(6), "Condenser (unknown?) Nodes");
                // Condenser Inlet node name is necessary for Water Cooled
                if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + " is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(6)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(6) + " is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            }

            EngineDrivenChiller(ChillerNum).MinPartLoadRat = rNumericArgs(3);
            EngineDrivenChiller(ChillerNum).MaxPartLoadRat = rNumericArgs(4);
            EngineDrivenChiller(ChillerNum).OptPartLoadRat = rNumericArgs(5);
            EngineDrivenChiller(ChillerNum).TempDesCondIn = rNumericArgs(6);
            EngineDrivenChiller(ChillerNum).TempRiseCoef = rNumericArgs(7);
            EngineDrivenChiller(ChillerNum).TempDesEvapOut = rNumericArgs(8);
            EngineDrivenChiller(ChillerNum).Base.EvapVolFlowRate = rNumericArgs(9);
            if (EngineDrivenChiller(ChillerNum).Base.EvapVolFlowRate == AutoSize) {
                EngineDrivenChiller(ChillerNum).Base.EvapVolFlowRateWasAutoSized = true;
            }
            EngineDrivenChiller(ChillerNum).Base.CondVolFlowRate = rNumericArgs(10);
            if (EngineDrivenChiller(ChillerNum).Base.CondVolFlowRate == AutoSize) {
                if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                    EngineDrivenChiller(ChillerNum).Base.CondVolFlowRateWasAutoSized = true;
                }
            }
            EngineDrivenChiller(ChillerNum).CapRatCoef(1) = rNumericArgs(11);
            EngineDrivenChiller(ChillerNum).CapRatCoef(2) = rNumericArgs(12);
            EngineDrivenChiller(ChillerNum).CapRatCoef(3) = rNumericArgs(13);
            if ((rNumericArgs(11) + rNumericArgs(12) + rNumericArgs(13)) == 0.0) {
                ShowSevereError(cCurrentModuleObject + ": Sum of Capacity Ratio Coef = 0.0, chiller=" + cAlphaArgs(1));
                ErrorsFound = true;
            }
            EngineDrivenChiller(ChillerNum).PowerRatCoef(1) = rNumericArgs(14);
            EngineDrivenChiller(ChillerNum).PowerRatCoef(2) = rNumericArgs(15);
            EngineDrivenChiller(ChillerNum).PowerRatCoef(3) = rNumericArgs(16);
            EngineDrivenChiller(ChillerNum).FullLoadCoef(1) = rNumericArgs(17);
            EngineDrivenChiller(ChillerNum).FullLoadCoef(2) = rNumericArgs(18);
            EngineDrivenChiller(ChillerNum).FullLoadCoef(3) = rNumericArgs(19);
            EngineDrivenChiller(ChillerNum).TempLowLimitEvapOut = rNumericArgs(20);

            // Load Special EngineDriven Chiller Curve Fit Inputs
            EngineDrivenChiller(ChillerNum).ClngLoadtoFuelCurve = GetCurveIndex(cAlphaArgs(7)); // convert curve name to number
            if (EngineDrivenChiller(ChillerNum).ClngLoadtoFuelCurve == 0) {
                ShowSevereError("Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).RecJacHeattoFuelCurve = GetCurveIndex(cAlphaArgs(8)); // convert curve name to number
            if (EngineDrivenChiller(ChillerNum).RecJacHeattoFuelCurve == 0) {
                ShowSevereError("Invalid " + cAlphaFieldNames(8) + '=' + cAlphaArgs(8));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).RecLubeHeattoFuelCurve = GetCurveIndex(cAlphaArgs(9)); // convert curve name to number
            if (EngineDrivenChiller(ChillerNum).RecLubeHeattoFuelCurve == 0) {
                ShowSevereError("Invalid " + cAlphaFieldNames(9) + '=' + cAlphaArgs(9));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).TotExhausttoFuelCurve = GetCurveIndex(cAlphaArgs(10)); // convert curve name to number
            if (EngineDrivenChiller(ChillerNum).TotExhausttoFuelCurve == 0) {
                ShowSevereError("Invalid " + cAlphaFieldNames(10) + '=' + cAlphaArgs(10));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).ExhaustTempCurve = GetCurveIndex(cAlphaArgs(11)); // convert curve name to number
            if (EngineDrivenChiller(ChillerNum).ExhaustTempCurve == 0) {
                ShowSevereError("Invalid " + cAlphaFieldNames(11) + '=' + cAlphaArgs(11));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).UACoef(1) = rNumericArgs(21);
            EngineDrivenChiller(ChillerNum).UACoef(2) = rNumericArgs(22);

            EngineDrivenChiller(ChillerNum).MaxExhaustperPowerOutput = rNumericArgs(23);
            EngineDrivenChiller(ChillerNum).DesignMinExitGasTemp = rNumericArgs(24);

            EngineDrivenChiller(ChillerNum).FuelType = cAlphaArgs(12);

            {
                auto const SELECT_CASE_var(cAlphaArgs(12));

                if ((SELECT_CASE_var == "Gas") || (SELECT_CASE_var == "NATURALGAS") || (SELECT_CASE_var == "NATURAL GAS")) {
                    EngineDrivenChiller(ChillerNum).FuelType = "Gas";

                } else if (SELECT_CASE_var == "DIESEL") {
                    EngineDrivenChiller(ChillerNum).FuelType = "Diesel";

                } else if (SELECT_CASE_var == "GASOLINE") {
                    EngineDrivenChiller(ChillerNum).FuelType = "Gasoline";

                } else if ((SELECT_CASE_var == "FUEL OIL #1") || (SELECT_CASE_var == "FUELOIL#1") || (SELECT_CASE_var == "FUEL OIL") ||
                           (SELECT_CASE_var == "DISTILLATE OIL")) {
                    EngineDrivenChiller(ChillerNum).FuelType = "FuelOil#1";

                } else if ((SELECT_CASE_var == "FUEL OIL #2") || (SELECT_CASE_var == "FUELOIL#2") || (SELECT_CASE_var == "RESIDUAL OIL")) {
                    EngineDrivenChiller(ChillerNum).FuelType = "FuelOil#2";

                } else if ((SELECT_CASE_var == "Propane") || (SELECT_CASE_var == "LPG") || (SELECT_CASE_var == "PROPANEGAS") ||
                           (SELECT_CASE_var == "PROPANE GAS")) {
                    EngineDrivenChiller(ChillerNum).FuelType = "Propane";

                } else if (SELECT_CASE_var == "OTHERFUEL1") {
                    EngineDrivenChiller(ChillerNum).FuelType = "OtherFuel1";

                } else if (SELECT_CASE_var == "OTHERFUEL2") {
                    EngineDrivenChiller(ChillerNum).FuelType = "OtherFuel2";

                } else {
                    ShowSevereError("Invalid " + cAlphaFieldNames(12) + '=' + cAlphaArgs(12));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ShowContinueError(
                        "Valid choices are Electricity, NaturalGas, PropaneGas, Diesel, Gasoline, FuelOil#1, FuelOil#2,OtherFuel1 or OtherFuel2");
                    ErrorsFound = true;
                }
            }

            EngineDrivenChiller(ChillerNum).FuelHeatingValue = rNumericArgs(25);

            // add support of autosize to this.

            EngineDrivenChiller(ChillerNum).DesignHeatRecVolFlowRate = rNumericArgs(26);
            if (EngineDrivenChiller(ChillerNum).DesignHeatRecVolFlowRate > 0.0 ||
                EngineDrivenChiller(ChillerNum).DesignHeatRecVolFlowRate == DataSizing::AutoSize) {
                EngineDrivenChiller(ChillerNum).HeatRecActive = true;
                EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(13), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 3, ObjectIsNotParent);
                if (EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum == 0) {
                    ShowSevereError("Invalid " + cAlphaFieldNames(13) + '=' + cAlphaArgs(13));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
                EngineDrivenChiller(ChillerNum).HeatRecOutletNodeNum = GetOnlySingleNode(cAlphaArgs(14),
                                                                                         ErrorsFound,
                                                                                         cCurrentModuleObject,
                                                                                         cAlphaArgs(1),
                                                                                         NodeType_Water,
                                                                                         NodeConnectionType_Outlet,
                                                                                         3,
                                                                                         ObjectIsNotParent);
                if (EngineDrivenChiller(ChillerNum).HeatRecOutletNodeNum == 0) {
                    ShowSevereError("Invalid " + cAlphaFieldNames(14) + '=' + cAlphaArgs(14));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(13), cAlphaArgs(14), "Heat Recovery Nodes");
                if (EngineDrivenChiller(ChillerNum).DesignHeatRecVolFlowRate == DataSizing::AutoSize) {
                    EngineDrivenChiller(ChillerNum).DesignHeatRecVolFlowRateWasAutoSized = true;
                } else {
                    RegisterPlantCompDesignFlow(EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum,
                                                EngineDrivenChiller(ChillerNum).DesignHeatRecVolFlowRate);
                }

                // Condenser flow rate must be specified for heat reclaim
                if (EngineDrivenChiller(ChillerNum).Base.CondenserType == AirCooled ||
                    EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    if (EngineDrivenChiller(ChillerNum).Base.CondVolFlowRate <= 0.0) {
                        ShowSevereError("Invalid " + cNumericFieldNames(10) + '=' + RoundSigDigits(rNumericArgs(10), 6));
                        ShowSevereError("Condenser fluid flow rate must be specified for Heat Reclaim applications.");
                        ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                        ErrorsFound = true;
                    }
                }

            } else {

                EngineDrivenChiller(ChillerNum).HeatRecActive = false;
                EngineDrivenChiller(ChillerNum).DesignHeatRecMassFlowRate = 0.0;
                EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum = 0;
                EngineDrivenChiller(ChillerNum).HeatRecOutletNodeNum = 0;
                // if heat recovery is not used, don't care about condenser flow rate for air/evap-cooled equip.
                if (EngineDrivenChiller(ChillerNum).Base.CondenserType == AirCooled ||
                    EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    EngineDrivenChiller(ChillerNum).Base.CondVolFlowRate = 0.0011; // set to avoid errors in calc routine
                }
                if ((!lAlphaFieldBlanks(13)) || (!lAlphaFieldBlanks(14))) {
                    ShowWarningError("Since Design Heat Flow Rate = 0.0, Heat Recovery inactive for " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ShowContinueError("However, Node names were specified for Heat Recovery inlet or outlet nodes");
                }
            }

            {
                auto const SELECT_CASE_var(cAlphaArgs(15));
                if (SELECT_CASE_var == "CONSTANTFLOW") {
                    EngineDrivenChiller(ChillerNum).Base.FlowMode = ConstantFlow;
                } else if (SELECT_CASE_var == "VARIABLEFLOW") {
                    EngineDrivenChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                    ShowWarningError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(15) + '=' + cAlphaArgs(15));
                    ShowContinueError("Key choice is now called \"LeavingSetpointModulated\" and the simulation continues");
                } else if (SELECT_CASE_var == "LEAVINGSETPOINTMODULATED") {
                    EngineDrivenChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                } else if (SELECT_CASE_var == "NOTMODULATED") {
                    EngineDrivenChiller(ChillerNum).Base.FlowMode = NotModulated;
                } else {
                    ShowSevereError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(15) + '=' + cAlphaArgs(15));
                    ShowContinueError("Available choices are ConstantFlow, NotModulated, or LeavingSetpointModulated");
                    ShowContinueError("Flow mode NotModulated is assumed and the simulation continues.");
                    EngineDrivenChiller(ChillerNum).Base.FlowMode = NotModulated;
                }
            }

            EngineDrivenChiller(ChillerNum).HeatRecMaxTemp = rNumericArgs(27);
            EngineDrivenChiller(ChillerNum).Base.SizFac = rNumericArgs(28);
            if (EngineDrivenChiller(ChillerNum).Base.SizFac <= 0.0) EngineDrivenChiller(ChillerNum).Base.SizFac = 1.0;

            //   Basin heater power as a function of temperature must be greater than or equal to 0
            EngineDrivenChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff = rNumericArgs(29);
            if (rNumericArgs(29) < 0.0) {
                ShowSevereError(cCurrentModuleObject + ", \"" + EngineDrivenChiller(ChillerNum).Base.Name +
                                "\" TRIM(cNumericFieldNames(29)) must be >= 0");
                ErrorsFound = true;
            }

            EngineDrivenChiller(ChillerNum).Base.BasinHeaterSetPointTemp = rNumericArgs(30);

            if (EngineDrivenChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                if (NumNums < 30) {
                    EngineDrivenChiller(ChillerNum).Base.BasinHeaterSetPointTemp = 2.0;
                }
                if (EngineDrivenChiller(ChillerNum).Base.BasinHeaterSetPointTemp < 2.0) {
                    ShowWarningError(cCurrentModuleObject + ":\"" + EngineDrivenChiller(ChillerNum).Base.Name + "\", " + cNumericFieldNames(30) +
                                     " is less than 2 deg C. Freezing could occur.");
                }
            }

            if (!lAlphaFieldBlanks(16)) {
                EngineDrivenChiller(ChillerNum).Base.BasinHeaterSchedulePtr = GetScheduleIndex(cAlphaArgs(16));
                if (EngineDrivenChiller(ChillerNum).Base.BasinHeaterSchedulePtr == 0) {
                    ShowWarningError(cCurrentModuleObject + ", \"" + EngineDrivenChiller(ChillerNum).Base.Name + "\" TRIM(cAlphaFieldNames(16)) \"" +
                                     cAlphaArgs(16) + "\" was not found. Basin heater operation will not be modeled and the simulation continues");
                }
            }

            if (NumNums > 30) {
                if (!lNumericFieldBlanks(31)) {
                    EngineDrivenChiller(ChillerNum).HeatRecCapacityFraction = rNumericArgs(31);
                } else {
                    EngineDrivenChiller(ChillerNum).HeatRecCapacityFraction = 1.0;
                }
            } else {
                EngineDrivenChiller(ChillerNum).HeatRecCapacityFraction = 1.0;
            }
        }

        if (ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + cCurrentModuleObject);
        }

        for (ChillerNum = 1; ChillerNum <= NumEngineDrivenChillers; ++ChillerNum) {
            SetupOutputVariable("Chiller Drive Shaft Power",
                                OutputProcessor::Unit::W,
                                EngineDrivenChillerReport(ChillerNum).Base.Power,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Drive Shaft Energy",
                                OutputProcessor::Unit::J,
                                EngineDrivenChillerReport(ChillerNum).Base.Energy,
                                "System",
                                "Sum",
                                EngineDrivenChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Evaporator Cooling Rate",
                                OutputProcessor::Unit::W,
                                EngineDrivenChillerReport(ChillerNum).Base.QEvap,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Cooling Energy",
                                OutputProcessor::Unit::J,
                                EngineDrivenChillerReport(ChillerNum).Base.EvapEnergy,
                                "System",
                                "Sum",
                                EngineDrivenChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "CHILLERS",
                                _,
                                "Plant");
            SetupOutputVariable("Chiller Evaporator Inlet Temperature",
                                OutputProcessor::Unit::C,
                                EngineDrivenChillerReport(ChillerNum).Base.EvapInletTemp,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Outlet Temperature",
                                OutputProcessor::Unit::C,
                                EngineDrivenChillerReport(ChillerNum).Base.EvapOutletTemp,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                EngineDrivenChillerReport(ChillerNum).Base.Evapmdot,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Condenser Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                EngineDrivenChillerReport(ChillerNum).Base.QCond,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Condenser Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                EngineDrivenChillerReport(ChillerNum).Base.CondEnergy,
                                "System",
                                "Sum",
                                EngineDrivenChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATREJECTION",
                                _,
                                "Plant");

            // Condenser mass flow and outlet temp are valid for Water Cooled
            if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    EngineDrivenChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    EngineDrivenChillerReport(ChillerNum).Base.CondOutletTemp,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Mass Flow Rate",
                                    OutputProcessor::Unit::kg_s,
                                    EngineDrivenChillerReport(ChillerNum).Base.Condmdot,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
            } else if (EngineDrivenChiller(ChillerNum).Base.CondenserType == AirCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    EngineDrivenChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
            } else if (EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    EngineDrivenChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                if (EngineDrivenChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                    SetupOutputVariable("Chiller Basin Heater Electric Power",
                                        OutputProcessor::Unit::W,
                                        EngineDrivenChillerReport(ChillerNum).Base.BasinHeaterPower,
                                        "System",
                                        "Average",
                                        EngineDrivenChiller(ChillerNum).Base.Name);
                    SetupOutputVariable("Chiller Basin Heater Electric Energy",
                                        OutputProcessor::Unit::J,
                                        EngineDrivenChillerReport(ChillerNum).Base.BasinHeaterConsumption,
                                        "System",
                                        "Sum",
                                        EngineDrivenChiller(ChillerNum).Base.Name,
                                        _,
                                        "Electric",
                                        "CHILLERS",
                                        _,
                                        "Plant");
                }
            }

            SetupOutputVariable("Chiller " + EngineDrivenChiller(ChillerNum).FuelType + " Rate",
                                OutputProcessor::Unit::W,
                                EngineDrivenChillerReport(ChillerNum).FuelEnergyUseRate,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller " + EngineDrivenChiller(ChillerNum).FuelType + " Energy",
                                OutputProcessor::Unit::J,
                                EngineDrivenChillerReport(ChillerNum).FuelEnergy,
                                "System",
                                "Sum",
                                EngineDrivenChiller(ChillerNum).Base.Name,
                                _,
                                EngineDrivenChiller(ChillerNum).FuelType,
                                "Cooling",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller COP",
                                OutputProcessor::Unit::W_W,
                                EngineDrivenChillerReport(ChillerNum).FuelCOP,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller " + EngineDrivenChiller(ChillerNum).FuelType + " Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                EngineDrivenChillerReport(ChillerNum).FuelMdot,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Exhaust Temperature",
                                OutputProcessor::Unit::C,
                                EngineDrivenChillerReport(ChillerNum).ExhaustStackTemp,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Heat Recovery Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                EngineDrivenChillerReport(ChillerNum).HeatRecMdot,
                                "System",
                                "Average",
                                EngineDrivenChiller(ChillerNum).Base.Name);

            if (EngineDrivenChiller(ChillerNum).HeatRecActive) {
                // need to only report if heat recovery active
                SetupOutputVariable("Chiller Jacket Recovered Heat Rate",
                                    OutputProcessor::Unit::W,
                                    EngineDrivenChillerReport(ChillerNum).QJacketRecovered,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Jacket Recovered Heat Energy",
                                    OutputProcessor::Unit::J,
                                    EngineDrivenChillerReport(ChillerNum).JacketEnergyRec,
                                    "System",
                                    "Sum",
                                    EngineDrivenChiller(ChillerNum).Base.Name,
                                    _,
                                    "ENERGYTRANSFER",
                                    "HEATRECOVERY",
                                    _,
                                    "Plant");

                SetupOutputVariable("Chiller Lube Recovered Heat Rate",
                                    OutputProcessor::Unit::W,
                                    EngineDrivenChillerReport(ChillerNum).QLubeOilRecovered,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Lube Recovered Heat Energy",
                                    OutputProcessor::Unit::J,
                                    EngineDrivenChillerReport(ChillerNum).LubeOilEnergyRec,
                                    "System",
                                    "Sum",
                                    EngineDrivenChiller(ChillerNum).Base.Name,
                                    _,
                                    "ENERGYTRANSFER",
                                    "HEATRECOVERY",
                                    _,
                                    "Plant");

                SetupOutputVariable("Chiller Exhaust Recovered Heat Rate",
                                    OutputProcessor::Unit::W,
                                    EngineDrivenChillerReport(ChillerNum).QExhaustRecovered,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Exhaust Recovered Heat Energy",
                                    OutputProcessor::Unit::J,
                                    EngineDrivenChillerReport(ChillerNum).ExhaustEnergyRec,
                                    "System",
                                    "Sum",
                                    EngineDrivenChiller(ChillerNum).Base.Name,
                                    _,
                                    "ENERGYTRANSFER",
                                    "HEATRECOVERY",
                                    _,
                                    "Plant");

                SetupOutputVariable("Chiller Total Recovered Heat Rate",
                                    OutputProcessor::Unit::W,
                                    EngineDrivenChillerReport(ChillerNum).QTotalHeatRecovered,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Total Recovered Heat Energy",
                                    OutputProcessor::Unit::J,
                                    EngineDrivenChillerReport(ChillerNum).TotalHeatEnergyRec,
                                    "System",
                                    "Sum",
                                    EngineDrivenChiller(ChillerNum).Base.Name);

                SetupOutputVariable("Chiller Heat Recovery Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    EngineDrivenChillerReport(ChillerNum).HeatRecInletTemp,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);

                SetupOutputVariable("Chiller Heat Recovery Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    EngineDrivenChillerReport(ChillerNum).HeatRecOutletTemp,
                                    "System",
                                    "Average",
                                    EngineDrivenChiller(ChillerNum).Base.Name);
            }
            if (AnyEnergyManagementSystemInModel) {
                SetupEMSInternalVariable(
                    "Chiller Nominal Capacity", EngineDrivenChiller(ChillerNum).Base.Name, "[W]", EngineDrivenChiller(ChillerNum).Base.NomCap);
            }
        }
    }

    void GetGTChillerInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher / Brandon Anderson
        //       DATE WRITTEN:    September 2000

        // PURPOSE OF THIS SUBROUTINE:
        // This routine will get the input
        // required by the GT Chiller model.

        // METHODOLOGY EMPLOYED:
        // EnergyPlus input processor

        // Using/Aliasing
        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using BranchNodeConnections::TestCompSet;
        using DataGlobals::AnyEnergyManagementSystemInModel;
        using DataSizing::AutoSize;
        using General::RoundSigDigits;
        using GlobalNames::VerifyUniqueChillerName;
        using NodeInputManager::GetOnlySingleNode;
        using OutAirNodeManager::CheckAndAddAirNodeNumber;
        using PlantUtilities::RegisterPlantCompDesignFlow;
        using ScheduleManager::GetScheduleIndex;

        // Locals
        // PARAMETERS
        static std::string const RoutineName("GetGTChillerInput: "); // include trailing blank space
        // LOCAL VARIABLES
        int ChillerNum; // chiller counter
        int NumAlphas;  // Number of elements in the alpha array
        int NumNums;    // Number of elements in the numeric array
        int IOStat;     // IO Status when calling get input subroutine
        bool ErrorsFound(false);
        bool Okay;

        // FLOW
        cCurrentModuleObject = "Chiller:CombustionTurbine";
        NumGTChillers = inputProcessor->getNumObjectsFound(cCurrentModuleObject);

        if (NumGTChillers <= 0) {
            ShowSevereError("No " + cCurrentModuleObject + " equipment specified in input file");
            ErrorsFound = true;
        }
        // See if load distribution manager has already gotten the input
        if (allocated(GTChiller)) return;

        // ALLOCATE ARRAYS
        GTChiller.allocate(NumGTChillers);
        GTChillerReport.allocate(NumGTChillers);

        for (ChillerNum = 1; ChillerNum <= NumGTChillers; ++ChillerNum) {
            inputProcessor->getObjectItem(cCurrentModuleObject,
                                          ChillerNum,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          lNumericFieldBlanks,
                                          lAlphaFieldBlanks,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(cAlphaArgs(1), cCurrentModuleObject, ErrorsFound);

            // ErrorsFound will be set to True if problem was found, left untouched otherwise 
            VerifyUniqueChillerName(cCurrentModuleObject, cAlphaArgs(1), ErrorsFound, cCurrentModuleObject + " Name");
            
            GTChiller(ChillerNum).Base.Name = cAlphaArgs(1);

            GTChiller(ChillerNum).Base.NomCap = rNumericArgs(1);

            if (GTChiller(ChillerNum).Base.NomCap == DataSizing::AutoSize) {
                GTChiller(ChillerNum).Base.NomCapWasAutoSized = true;
            }
            if (rNumericArgs(1) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(1) + '=' + RoundSigDigits(rNumericArgs(1), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            GTChiller(ChillerNum).Base.COP = rNumericArgs(2);
            if (rNumericArgs(2) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(2) + '=' + RoundSigDigits(rNumericArgs(2), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            if (cAlphaArgs(2) == "AIRCOOLED") {
                GTChiller(ChillerNum).Base.CondenserType = AirCooled;
            } else if (cAlphaArgs(2) == "WATERCOOLED") {
                GTChiller(ChillerNum).Base.CondenserType = WaterCooled;
            } else if (cAlphaArgs(2) == "EVAPORATIVELYCOOLED") {
                GTChiller(ChillerNum).Base.CondenserType = EvapCooled;
            } else {
                ShowSevereError("Invalid " + cAlphaFieldNames(2) + '=' + cAlphaArgs(2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            GTChiller(ChillerNum).Base.EvapInletNodeNum = GetOnlySingleNode(
                cAlphaArgs(3), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 1, ObjectIsNotParent);
            GTChiller(ChillerNum).Base.EvapOutletNodeNum = GetOnlySingleNode(
                cAlphaArgs(4), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 1, ObjectIsNotParent);
            TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(3), cAlphaArgs(4), "Chilled Water Nodes");

            if (GTChiller(ChillerNum).Base.CondenserType == AirCooled || GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                // Connection not required for air or evap cooled condenser
                // If the condenser inlet is blank for air cooled and evap cooled condensers then supply a generic name
                // since it is not used elsewhere for connection
                if (lAlphaFieldBlanks(5)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 21) { // protect against long name leading to > 100 chars
                        cAlphaArgs(5) = cAlphaArgs(1) + " CONDENSER INLET NODE";
                    } else {
                        cAlphaArgs(5) = cAlphaArgs(1).substr(0, 79) + " CONDENSER INLET NODE";
                    }
                }
                if (lAlphaFieldBlanks(6)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 22) { // protect against long name leading to > 100 chars
                        cAlphaArgs(6) = cAlphaArgs(1) + " CONDENSER OUTLET NODE";
                    } else {
                        cAlphaArgs(6) = cAlphaArgs(1).substr(0, 78) + " CONDENSER OUTLET NODE";
                    }
                }

                GTChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                ErrorsFound,
                                                                                cCurrentModuleObject,
                                                                                cAlphaArgs(1),
                                                                                NodeType_Air,
                                                                                NodeConnectionType_OutsideAirReference,
                                                                                2,
                                                                                ObjectIsNotParent);
                CheckAndAddAirNodeNumber(GTChiller(ChillerNum).Base.CondInletNodeNum, Okay);
                if (!Okay) {
                    ShowWarningError(cCurrentModuleObject + ", Adding OutdoorAir:Node=" + cAlphaArgs(5));
                }

                GTChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(6), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Air, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
            } else { // WaterCooled CondenserType
                GTChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                ErrorsFound,
                                                                                cCurrentModuleObject,
                                                                                cAlphaArgs(1),
                                                                                NodeType_Unknown,
                                                                                NodeConnectionType_Inlet,
                                                                                2,
                                                                                ObjectIsNotParent);
                GTChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(cAlphaArgs(6),
                                                                                 ErrorsFound,
                                                                                 cCurrentModuleObject,
                                                                                 cAlphaArgs(1),
                                                                                 NodeType_Unknown,
                                                                                 NodeConnectionType_Outlet,
                                                                                 2,
                                                                                 ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(5), cAlphaArgs(6), "Condenser (unknown?) Nodes");
                // Condenser Inlet node name is necessary for Water Cooled
                if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + " is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(6)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(6) + " is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            }

            GTChiller(ChillerNum).MinPartLoadRat = rNumericArgs(3);
            GTChiller(ChillerNum).MaxPartLoadRat = rNumericArgs(4);
            GTChiller(ChillerNum).OptPartLoadRat = rNumericArgs(5);
            GTChiller(ChillerNum).TempDesCondIn = rNumericArgs(6);
            GTChiller(ChillerNum).TempRiseCoef = rNumericArgs(7);
            GTChiller(ChillerNum).TempDesEvapOut = rNumericArgs(8);
            GTChiller(ChillerNum).Base.EvapVolFlowRate = rNumericArgs(9);
            if (GTChiller(ChillerNum).Base.EvapVolFlowRate == DataSizing::AutoSize) {
                GTChiller(ChillerNum).Base.EvapVolFlowRateWasAutoSized = true;
            }

            GTChiller(ChillerNum).Base.CondVolFlowRate = rNumericArgs(10);
            if (GTChiller(ChillerNum).Base.CondVolFlowRate == DataSizing::AutoSize) {
                if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                    GTChiller(ChillerNum).Base.CondVolFlowRateWasAutoSized = true;
                }
            }
            GTChiller(ChillerNum).CapRatCoef(1) = rNumericArgs(11);
            GTChiller(ChillerNum).CapRatCoef(2) = rNumericArgs(12);
            GTChiller(ChillerNum).CapRatCoef(3) = rNumericArgs(13);
            if ((rNumericArgs(11) + rNumericArgs(12) + rNumericArgs(13)) == 0.0) {
                ShowSevereError(cCurrentModuleObject + ": Sum of Capacity Ratio Coef = 0.0, chiller=" + cAlphaArgs(1));
                ErrorsFound = true;
            }
            GTChiller(ChillerNum).PowerRatCoef(1) = rNumericArgs(14);
            GTChiller(ChillerNum).PowerRatCoef(2) = rNumericArgs(15);
            GTChiller(ChillerNum).PowerRatCoef(3) = rNumericArgs(16);
            GTChiller(ChillerNum).FullLoadCoef(1) = rNumericArgs(17);
            GTChiller(ChillerNum).FullLoadCoef(2) = rNumericArgs(18);
            GTChiller(ChillerNum).FullLoadCoef(3) = rNumericArgs(19);
            GTChiller(ChillerNum).TempLowLimitEvapOut = rNumericArgs(20);

            // Load Special GT Chiller Input

            GTChiller(ChillerNum).PLBasedFuelInputCoef(1) = rNumericArgs(21);
            GTChiller(ChillerNum).PLBasedFuelInputCoef(2) = rNumericArgs(22);
            GTChiller(ChillerNum).PLBasedFuelInputCoef(3) = rNumericArgs(23);

            GTChiller(ChillerNum).TempBasedFuelInputCoef(1) = rNumericArgs(24);
            GTChiller(ChillerNum).TempBasedFuelInputCoef(2) = rNumericArgs(25);
            GTChiller(ChillerNum).TempBasedFuelInputCoef(3) = rNumericArgs(26);

            GTChiller(ChillerNum).ExhaustFlowCoef(1) = rNumericArgs(27);
            GTChiller(ChillerNum).ExhaustFlowCoef(2) = rNumericArgs(28);
            GTChiller(ChillerNum).ExhaustFlowCoef(3) = rNumericArgs(29);

            GTChiller(ChillerNum).PLBasedExhaustTempCoef(1) = rNumericArgs(30);
            GTChiller(ChillerNum).PLBasedExhaustTempCoef(2) = rNumericArgs(31);
            GTChiller(ChillerNum).PLBasedExhaustTempCoef(3) = rNumericArgs(32);

            GTChiller(ChillerNum).TempBasedExhaustTempCoef(1) = rNumericArgs(33);
            GTChiller(ChillerNum).TempBasedExhaustTempCoef(2) = rNumericArgs(34);
            GTChiller(ChillerNum).TempBasedExhaustTempCoef(3) = rNumericArgs(35);

            GTChiller(ChillerNum).HeatRecLubeEnergyCoef(1) = rNumericArgs(36);
            GTChiller(ChillerNum).HeatRecLubeEnergyCoef(2) = rNumericArgs(37);
            GTChiller(ChillerNum).HeatRecLubeEnergyCoef(3) = rNumericArgs(38);

            GTChiller(ChillerNum).UAtoCapCoef(1) = rNumericArgs(39);
            GTChiller(ChillerNum).UAtoCapCoef(2) = rNumericArgs(40);

            GTChiller(ChillerNum).GTEngineCapacity = rNumericArgs(41);
            if (GTChiller(ChillerNum).GTEngineCapacity == AutoSize) {
                GTChiller(ChillerNum).GTEngineCapacityWasAutoSized = true;
            }
            GTChiller(ChillerNum).MaxExhaustperGTPower = rNumericArgs(42);
            GTChiller(ChillerNum).DesignSteamSatTemp = rNumericArgs(43);
            GTChiller(ChillerNum).FuelHeatingValue = rNumericArgs(44);

            // Get the Heat Recovery information
            // handle autosize
            GTChiller(ChillerNum).DesignHeatRecVolFlowRate = rNumericArgs(45);
            if (GTChiller(ChillerNum).DesignHeatRecVolFlowRate > 0.0 || GTChiller(ChillerNum).DesignHeatRecVolFlowRate == DataSizing::AutoSize) {
                GTChiller(ChillerNum).HeatRecActive = true;
                GTChiller(ChillerNum).HeatRecInletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(7), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 3, ObjectIsNotParent);
                if (GTChiller(ChillerNum).HeatRecInletNodeNum == 0) {
                    ShowSevereError("Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
                GTChiller(ChillerNum).HeatRecOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(8), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 3, ObjectIsNotParent);
                if (GTChiller(ChillerNum).HeatRecOutletNodeNum == 0) {
                    ShowSevereError("Invalid " + cAlphaFieldNames(8) + '=' + cAlphaArgs(8));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(7), cAlphaArgs(8), "Heat Recovery Nodes");

                if (GTChiller(ChillerNum).DesignHeatRecVolFlowRate == DataSizing::AutoSize) {
                    GTChiller(ChillerNum).DesignHeatRecVolFlowRateWasAutoSized = true;
                } else {
                    RegisterPlantCompDesignFlow(GTChiller(ChillerNum).HeatRecInletNodeNum, GTChiller(ChillerNum).DesignHeatRecVolFlowRate);
                }

                // Condenser flow rate must be specified for heat reclaim, but Why couldn't this be okay??
                if (GTChiller(ChillerNum).Base.CondenserType == AirCooled || GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    if (GTChiller(ChillerNum).Base.CondVolFlowRate <= 0.0) {
                        ShowSevereError("Invalid " + cNumericFieldNames(10) + '=' + RoundSigDigits(rNumericArgs(10), 6));
                        ShowSevereError("Condenser fluid flow rate must be specified for Heat Reclaim applications.");
                        ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                        ErrorsFound = true;
                    }
                }

            } else {
                GTChiller(ChillerNum).HeatRecActive = false;
                GTChiller(ChillerNum).DesignHeatRecMassFlowRate = 0.0;
                GTChiller(ChillerNum).HeatRecInletNodeNum = 0;
                GTChiller(ChillerNum).HeatRecOutletNodeNum = 0;
                if ((!lAlphaFieldBlanks(7)) || (!lAlphaFieldBlanks(8))) {
                    ShowWarningError("Since Design Heat Flow Rate = 0.0, Heat Recovery inactive for " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ShowContinueError("However, Node names were specified for heat recovery inlet or outlet nodes");
                }
                if (GTChiller(ChillerNum).Base.CondenserType == AirCooled || GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    GTChiller(ChillerNum).Base.CondVolFlowRate = 0.0011; // set to avoid errors in calc routine
                }
            }

            {
                auto const SELECT_CASE_var(cAlphaArgs(9));
                if (SELECT_CASE_var == "CONSTANTFLOW") {
                    GTChiller(ChillerNum).Base.FlowMode = ConstantFlow;
                } else if (SELECT_CASE_var == "VARIABLEFLOW") {
                    GTChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                    ShowWarningError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(9) + '=' + cAlphaArgs(9));
                    ShowContinueError("Key choice is now called \"LeavingSetpointModulated\" and the simulation continues");
                } else if (SELECT_CASE_var == "LEAVINGSETPOINTMODULATED") {
                    GTChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                } else if (SELECT_CASE_var == "NOTMODULATED") {
                    GTChiller(ChillerNum).Base.FlowMode = NotModulated;
                } else {
                    ShowSevereError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(9) + '=' + cAlphaArgs(9));
                    ShowContinueError("Available choices are ConstantFlow, NotModulated, or LeavingSetpointModulated");
                    ShowContinueError("Flow mode NotModulated is assumed and the simulation continues.");
                    GTChiller(ChillerNum).Base.FlowMode = NotModulated;
                }
            }

            // Fuel Type Case Statement
            {
                auto const SELECT_CASE_var(cAlphaArgs(10));
                if ((SELECT_CASE_var == "GAS") || (SELECT_CASE_var == "NATURALGAS") || (SELECT_CASE_var == "NATURAL GAS")) {
                    GTChiller(ChillerNum).FuelType = "Gas";

                } else if (SELECT_CASE_var == "DIESEL") {
                    GTChiller(ChillerNum).FuelType = "Diesel";

                } else if (SELECT_CASE_var == "GASOLINE") {
                    GTChiller(ChillerNum).FuelType = "Gasoline";

                } else if ((SELECT_CASE_var == "FUEL OIL #1") || (SELECT_CASE_var == "FUELOIL#1") || (SELECT_CASE_var == "FUEL OIL") ||
                           (SELECT_CASE_var == "DISTILLATE OIL")) {
                    GTChiller(ChillerNum).FuelType = "FuelOil#1";

                } else if ((SELECT_CASE_var == "FUEL OIL #2") || (SELECT_CASE_var == "FUELOIL#2") || (SELECT_CASE_var == "RESIDUAL OIL")) {
                    GTChiller(ChillerNum).FuelType = "FuelOil#2";

                } else if ((SELECT_CASE_var == "PROPANE") || (SELECT_CASE_var == "LPG") || (SELECT_CASE_var == "PROPANEGAS") ||
                           (SELECT_CASE_var == "PROPANE GAS")) {
                    GTChiller(ChillerNum).FuelType = "Propane";

                } else if (SELECT_CASE_var == "OTHERFUEL1") {
                    GTChiller(ChillerNum).FuelType = "OtherFuel1";

                } else if (SELECT_CASE_var == "OTHERFUEL2") {
                    GTChiller(ChillerNum).FuelType = "OtherFuel2";

                } else {
                    ShowSevereError("Invalid " + cAlphaFieldNames(10) + '=' + cAlphaArgs(10));
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ShowContinueError(
                        "Valid choices are Electricity, NaturalGas, PropaneGas, Diesel, Gasoline, FuelOil#1, FuelOil#2,OtherFuel1 or OtherFuel2");
                    ErrorsFound = true;
                }
            }

            GTChiller(ChillerNum).HeatRecMaxTemp = rNumericArgs(46);
            GTChiller(ChillerNum).Base.SizFac = rNumericArgs(47);
            if (GTChiller(ChillerNum).Base.SizFac <= 0.0) GTChiller(ChillerNum).Base.SizFac = 1.0;

            //   Basin heater power as a function of temperature must be greater than or equal to 0
            GTChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff = rNumericArgs(48);
            if (rNumericArgs(48) < 0.0) {
                ShowSevereError(cCurrentModuleObject + "=\"" + GTChiller(ChillerNum).Base.Name + "\"" + cNumericFieldNames(48) + " must be >= 0");
                ErrorsFound = true;
            }

            GTChiller(ChillerNum).Base.BasinHeaterSetPointTemp = rNumericArgs(49);

            if (GTChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                if (NumNums < 49) {
                    GTChiller(ChillerNum).Base.BasinHeaterSetPointTemp = 2.0;
                }
                if (GTChiller(ChillerNum).Base.BasinHeaterSetPointTemp < 2.0) {
                    ShowWarningError(cCurrentModuleObject + ":\"" + GTChiller(ChillerNum).Base.Name + "\", " + cNumericFieldNames(49) +
                                     " is less than 2 deg C. Freezing could occur.");
                }
            }

            if (!lAlphaFieldBlanks(11)) {
                GTChiller(ChillerNum).Base.BasinHeaterSchedulePtr = GetScheduleIndex(cAlphaArgs(11));
                if (GTChiller(ChillerNum).Base.BasinHeaterSchedulePtr == 0) {
                    ShowWarningError(cCurrentModuleObject + ", \"" + GTChiller(ChillerNum).Base.Name + "\" TRIM(cAlphaFieldNames(11)) \"" +
                                     cAlphaArgs(11) + "\" was not found. Basin heater operation will not be modeled and the simulation continues");
                }
            }

            if (NumNums > 49) {
                if (!lNumericFieldBlanks(50)) {
                    GTChiller(ChillerNum).HeatRecCapacityFraction = rNumericArgs(50);
                } else {
                    GTChiller(ChillerNum).HeatRecCapacityFraction = 1.0;
                }
            } else {
                GTChiller(ChillerNum).HeatRecCapacityFraction = 1.0;
            }

            if (NumNums > 50) {
                if (!lNumericFieldBlanks(51)) {
                    GTChiller(ChillerNum).engineCapacityScalar = rNumericArgs(51);
                } else {
                    GTChiller(ChillerNum).engineCapacityScalar = 0.35;
                }
            } else {
                GTChiller(ChillerNum).engineCapacityScalar = 0.35;
            }
        }

        if (ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + cCurrentModuleObject);
        }

        for (ChillerNum = 1; ChillerNum <= NumGTChillers; ++ChillerNum) {
            SetupOutputVariable("Chiller Drive Shaft Power",
                                OutputProcessor::Unit::W,
                                GTChillerReport(ChillerNum).Base.Power,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Drive Shaft Energy",
                                OutputProcessor::Unit::J,
                                GTChillerReport(ChillerNum).Base.Energy,
                                "System",
                                "Sum",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Evaporator Cooling Rate",
                                OutputProcessor::Unit::W,
                                GTChillerReport(ChillerNum).Base.QEvap,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Cooling Energy",
                                OutputProcessor::Unit::J,
                                GTChillerReport(ChillerNum).Base.EvapEnergy,
                                "System",
                                "Sum",
                                GTChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "CHILLERS",
                                _,
                                "Plant");
            SetupOutputVariable("Chiller Evaporator Inlet Temperature",
                                OutputProcessor::Unit::C,
                                GTChillerReport(ChillerNum).Base.EvapInletTemp,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Outlet Temperature",
                                OutputProcessor::Unit::C,
                                GTChillerReport(ChillerNum).Base.EvapOutletTemp,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                GTChillerReport(ChillerNum).Base.Evapmdot,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Condenser Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                GTChillerReport(ChillerNum).Base.QCond,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Condenser Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                GTChillerReport(ChillerNum).Base.CondEnergy,
                                "System",
                                "Sum",
                                GTChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATREJECTION",
                                _,
                                "Plant");

            // Condenser mass flow and outlet temp are valid for water cooled
            if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GTChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    GTChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GTChillerReport(ChillerNum).Base.CondOutletTemp,
                                    "System",
                                    "Average",
                                    GTChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Mass Flow Rate",
                                    OutputProcessor::Unit::kg_s,
                                    GTChillerReport(ChillerNum).Base.Condmdot,
                                    "System",
                                    "Average",
                                    GTChiller(ChillerNum).Base.Name);
            } else if (GTChiller(ChillerNum).Base.CondenserType == AirCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GTChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    GTChiller(ChillerNum).Base.Name);
            } else if (GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    GTChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    GTChiller(ChillerNum).Base.Name);
                if (GTChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                    SetupOutputVariable("Chiller Basin Heater Electric Power",
                                        OutputProcessor::Unit::W,
                                        GTChillerReport(ChillerNum).Base.BasinHeaterPower,
                                        "System",
                                        "Average",
                                        GTChiller(ChillerNum).Base.Name);
                    SetupOutputVariable("Chiller Basin Heater Electric Energy",
                                        OutputProcessor::Unit::J,
                                        GTChillerReport(ChillerNum).Base.BasinHeaterConsumption,
                                        "System",
                                        "Sum",
                                        GTChiller(ChillerNum).Base.Name,
                                        _,
                                        "Electric",
                                        "CHILLERS",
                                        _,
                                        "Plant");
                }
            }

            SetupOutputVariable("Chiller Lube Recovered Heat Rate",
                                OutputProcessor::Unit::W,
                                GTChillerReport(ChillerNum).HeatRecLubeRate,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Lube Recovered Heat Energy",
                                OutputProcessor::Unit::J,
                                GTChillerReport(ChillerNum).HeatRecLubeEnergy,
                                "System",
                                "Sum",
                                GTChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HeatRecovery",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller " + GTChiller(ChillerNum).FuelType + " Rate",
                                OutputProcessor::Unit::W,
                                GTChillerReport(ChillerNum).FuelEnergyUsedRate,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller " + GTChiller(ChillerNum).FuelType + " Energy",
                                OutputProcessor::Unit::J,
                                GTChillerReport(ChillerNum).FuelEnergyUsed,
                                "System",
                                "Sum",
                                GTChiller(ChillerNum).Base.Name,
                                _,
                                GTChiller(ChillerNum).FuelType,
                                "Cooling",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller " + GTChiller(ChillerNum).FuelType + " Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                GTChillerReport(ChillerNum).FuelMassUsedRate,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller " + GTChiller(ChillerNum).FuelType + " Mass",
                                OutputProcessor::Unit::kg,
                                GTChillerReport(ChillerNum).FuelMassUsed,
                                "System",
                                "Sum",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Exhaust Temperature",
                                OutputProcessor::Unit::C,
                                GTChillerReport(ChillerNum).ExhaustStackTemp,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Heat Recovery Inlet Temperature",
                                OutputProcessor::Unit::C,
                                GTChillerReport(ChillerNum).HeatRecInletTemp,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Heat Recovery Outlet Temperature",
                                OutputProcessor::Unit::C,
                                GTChillerReport(ChillerNum).HeatRecOutletTemp,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Heat Recovery Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                GTChillerReport(ChillerNum).HeatRecMdot,
                                "System",
                                "Average",
                                GTChiller(ChillerNum).Base.Name);

            SetupOutputVariable(
                "Chiller COP", OutputProcessor::Unit::W_W, GTChillerReport(ChillerNum).FuelCOP, "System", "Average", GTChiller(ChillerNum).Base.Name);

            if (AnyEnergyManagementSystemInModel) {
                SetupEMSInternalVariable("Chiller Nominal Capacity", GTChiller(ChillerNum).Base.Name, "[W]", GTChiller(ChillerNum).Base.NomCap);
            }
        }
    }

    void GetConstCOPChillerInput()
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher
        //       DATE WRITTEN:    April 1998

        // PURPOSE OF THIS SUBROUTINE:!This routine will get the input
        // required by the PrimaryPlantLoopManager.  As such
        // it will interact with the Input Scanner to retrieve
        // information from the input file, count the number of
        // heating and cooling loops and begin to fill the
        // arrays associated with the type PlantLoopProps.

        // METHODOLOGY EMPLOYED: to be determined...

        // Using/Aliasing
        using namespace DataIPShortCuts; // Data for field names, blank numerics
        using BranchNodeConnections::TestCompSet;
        using GlobalNames::VerifyUniqueChillerName;
        using NodeInputManager::GetOnlySingleNode;
        using namespace OutputReportPredefined;
        using DataGlobals::AnyEnergyManagementSystemInModel;
        using DataSizing::AutoSize;
        using General::RoundSigDigits;
        using OutAirNodeManager::CheckAndAddAirNodeNumber;
        using ScheduleManager::GetScheduleIndex;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("GetConstCOPChillerInput: "); // include trailing blank space

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int ChillerNum;
        int NumAlphas; // Number of elements in the alpha array
        int NumNums;   // Number of elements in the numeric array
        int IOStat;    // IO Status when calling get input subroutine
        bool ErrorsFound(false);
        bool Okay;

        // GET NUMBER OF ALL EQUIPMENT TYPES
        cCurrentModuleObject = "Chiller:ConstantCOP";
        NumConstCOPChillers = inputProcessor->getNumObjectsFound(cCurrentModuleObject);

        if (NumConstCOPChillers <= 0) {
            ShowSevereError("No " + cCurrentModuleObject + " equipment specified in input file");
            ErrorsFound = true;
        }

        // See if load distribution manager has already gotten the input
        if (allocated(ConstCOPChiller)) return;

        ConstCOPChiller.allocate(NumConstCOPChillers);
        ConstCOPChillerReport.allocate(NumConstCOPChillers);

        // LOAD ARRAYS WITH BLAST ConstCOP CHILLER DATA
        for (ChillerNum = 1; ChillerNum <= NumConstCOPChillers; ++ChillerNum) {
            inputProcessor->getObjectItem(cCurrentModuleObject,
                                          ChillerNum,
                                          cAlphaArgs,
                                          NumAlphas,
                                          rNumericArgs,
                                          NumNums,
                                          IOStat,
                                          _,
                                          lAlphaFieldBlanks,
                                          cAlphaFieldNames,
                                          cNumericFieldNames);
            UtilityRoutines::IsNameEmpty(cAlphaArgs(1), cCurrentModuleObject, ErrorsFound);

            // ErrorsFound will be set to True if problem was found, left untouched otherwise 
            VerifyUniqueChillerName(cCurrentModuleObject, cAlphaArgs(1), ErrorsFound, cCurrentModuleObject + " Name");
            
            ConstCOPChiller(ChillerNum).Base.Name = cAlphaArgs(1);
            ConstCOPChiller(ChillerNum).Base.NomCap = rNumericArgs(1);
            if (ConstCOPChiller(ChillerNum).Base.NomCap == AutoSize) {
                ConstCOPChiller(ChillerNum).Base.NomCapWasAutoSized = true;
            }
            if (rNumericArgs(1) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(1) + '=' + RoundSigDigits(rNumericArgs(1), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }
            ConstCOPChiller(ChillerNum).Base.COP = rNumericArgs(2);
            if (rNumericArgs(2) == 0.0) {
                ShowSevereError("Invalid " + cNumericFieldNames(2) + '=' + RoundSigDigits(rNumericArgs(2), 2));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            // Set the Condenser Type from input
            if (cAlphaArgs(6) == "AIRCOOLED") {
                ConstCOPChiller(ChillerNum).Base.CondenserType = AirCooled;
            } else if (cAlphaArgs(6) == "EVAPORATIVELYCOOLED") {
                ConstCOPChiller(ChillerNum).Base.CondenserType = EvapCooled;
            } else if (cAlphaArgs(6) == "WATERCOOLED") {
                ConstCOPChiller(ChillerNum).Base.CondenserType = WaterCooled;
            } else {
                ShowSevereError("Invalid " + cAlphaFieldNames(6) + '=' + cAlphaArgs(6));
                ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                ErrorsFound = true;
            }

            ConstCOPChiller(ChillerNum).Base.EvapVolFlowRate = rNumericArgs(3);
            if (ConstCOPChiller(ChillerNum).Base.EvapVolFlowRate == AutoSize) {
                ConstCOPChiller(ChillerNum).Base.EvapVolFlowRateWasAutoSized = true;
            }
            if (ConstCOPChiller(ChillerNum).Base.CondenserType == AirCooled ||
                ConstCOPChiller(ChillerNum).Base.CondenserType == EvapCooled) { // Condenser flow rate not used for these cond types
                ConstCOPChiller(ChillerNum).Base.CondVolFlowRate = 0.0011;
            } else {
                ConstCOPChiller(ChillerNum).Base.CondVolFlowRate = rNumericArgs(4);
                if (ConstCOPChiller(ChillerNum).Base.CondVolFlowRate == AutoSize) {
                    if (ConstCOPChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                        ConstCOPChiller(ChillerNum).Base.CondVolFlowRateWasAutoSized = true;
                    }
                }
            }
            ConstCOPChiller(ChillerNum).Base.SizFac = rNumericArgs(5);

            ConstCOPChiller(ChillerNum).Base.EvapInletNodeNum = GetOnlySingleNode(
                cAlphaArgs(2), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 1, ObjectIsNotParent);
            ConstCOPChiller(ChillerNum).Base.EvapOutletNodeNum = GetOnlySingleNode(
                cAlphaArgs(3), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 1, ObjectIsNotParent);
            TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(2), cAlphaArgs(3), "Chilled Water Nodes");

            if (ConstCOPChiller(ChillerNum).Base.CondenserType == AirCooled || ConstCOPChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                // Connection not required for air or evap cooled condenser
                // If the condenser inlet is blank for air cooled and evap cooled condensers then supply a generic name
                //  since it is not used elsewhere for connection
                if (lAlphaFieldBlanks(4)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 21) { // protect against long name leading to > 100 chars
                        cAlphaArgs(4) = cAlphaArgs(1) + " CONDENSER INLET NODE";
                    } else {
                        cAlphaArgs(4) = cAlphaArgs(1).substr(0, 79) + " CONDENSER INLET NODE";
                    }
                }
                if (lAlphaFieldBlanks(5)) {
                    if (len(cAlphaArgs(1)) < MaxNameLength - 22) { // protect against long name leading to > 100 chars
                        cAlphaArgs(5) = cAlphaArgs(1) + " CONDENSER OUTLET NODE";
                    } else {
                        cAlphaArgs(5) = cAlphaArgs(1).substr(0, 78) + " CONDENSER OUTLET NODE";
                    }
                }

                ConstCOPChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(4),
                                                                                      ErrorsFound,
                                                                                      cCurrentModuleObject,
                                                                                      cAlphaArgs(1),
                                                                                      NodeType_Air,
                                                                                      NodeConnectionType_OutsideAirReference,
                                                                                      2,
                                                                                      ObjectIsNotParent);
                CheckAndAddAirNodeNumber(ConstCOPChiller(ChillerNum).Base.CondInletNodeNum, Okay);
                if (!Okay) {
                    ShowWarningError(cCurrentModuleObject + ", Adding OutdoorAir:Node=" + cAlphaArgs(4));
                }

                ConstCOPChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(5), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Air, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
            } else if (ConstCOPChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                ConstCOPChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(4), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Inlet, 2, ObjectIsNotParent);
                ConstCOPChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(
                    cAlphaArgs(5), ErrorsFound, cCurrentModuleObject, cAlphaArgs(1), NodeType_Water, NodeConnectionType_Outlet, 2, ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(4), cAlphaArgs(5), "Condenser Water Nodes");
                // Condenser Inlet node name is necessary for Water Cooled
                if (lAlphaFieldBlanks(4)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(4) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            } else {
                ConstCOPChiller(ChillerNum).Base.CondInletNodeNum = GetOnlySingleNode(cAlphaArgs(4),
                                                                                      ErrorsFound,
                                                                                      cCurrentModuleObject,
                                                                                      cAlphaArgs(1),
                                                                                      NodeType_Unknown,
                                                                                      NodeConnectionType_Inlet,
                                                                                      2,
                                                                                      ObjectIsNotParent);
                ConstCOPChiller(ChillerNum).Base.CondOutletNodeNum = GetOnlySingleNode(cAlphaArgs(5),
                                                                                       ErrorsFound,
                                                                                       cCurrentModuleObject,
                                                                                       cAlphaArgs(1),
                                                                                       NodeType_Unknown,
                                                                                       NodeConnectionType_Outlet,
                                                                                       2,
                                                                                       ObjectIsNotParent);
                TestCompSet(cCurrentModuleObject, cAlphaArgs(1), cAlphaArgs(4), cAlphaArgs(5), "Condenser (unknown?) Nodes");
                // Condenser Inlet node name is necessary for Water Cooled
                if (lAlphaFieldBlanks(4)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(4) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                } else if (lAlphaFieldBlanks(5)) {
                    ShowSevereError("Invalid, " + cAlphaFieldNames(5) + "is blank ");
                    ShowContinueError("Entered in " + cCurrentModuleObject + '=' + cAlphaArgs(1));
                    ErrorsFound = true;
                }
            }

            {
                auto const SELECT_CASE_var(cAlphaArgs(7));
                if (SELECT_CASE_var == "CONSTANTFLOW") {
                    ConstCOPChiller(ChillerNum).Base.FlowMode = ConstantFlow;
                } else if (SELECT_CASE_var == "VARIABLEFLOW") {
                    ConstCOPChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                    ShowWarningError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                    ShowContinueError("Key choice is now called \"LeavingSetpointModulated\" and the simulation continues");
                } else if (SELECT_CASE_var == "LEAVINGSETPOINTMODULATED") {
                    ConstCOPChiller(ChillerNum).Base.FlowMode = LeavingSetPointModulated;
                } else if (SELECT_CASE_var == "NOTMODULATED") {
                    ConstCOPChiller(ChillerNum).Base.FlowMode = NotModulated;
                } else {
                    ShowSevereError(RoutineName + cCurrentModuleObject + "=\"" + cAlphaArgs(1) + "\",");
                    ShowContinueError("Invalid " + cAlphaFieldNames(7) + '=' + cAlphaArgs(7));
                    ShowContinueError("Available choices are ConstantFlow, NotModulated, or LeavingSetpointModulated");
                    ShowContinueError("Flow mode NotModulated is assumed and the simulation continues.");
                    ConstCOPChiller(ChillerNum).Base.FlowMode = NotModulated;
                }
            }

            //   Basin heater power as a function of temperature must be greater than or equal to 0
            ConstCOPChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff = rNumericArgs(6);
            if (rNumericArgs(6) < 0.0) {
                ShowSevereError(cCurrentModuleObject + ", \"" + ConstCOPChiller(ChillerNum).Base.Name +
                                "\" TRIM(cNumericFieldNames(6)) must be >= 0");
                ErrorsFound = true;
            }

            ConstCOPChiller(ChillerNum).Base.BasinHeaterSetPointTemp = rNumericArgs(7);

            if (ConstCOPChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                if (NumNums < 7) {
                    ConstCOPChiller(ChillerNum).Base.BasinHeaterSetPointTemp = 2.0;
                }
                if (ConstCOPChiller(ChillerNum).Base.BasinHeaterSetPointTemp < 2.0) {
                    ShowWarningError(cCurrentModuleObject + ":\"" + ConstCOPChiller(ChillerNum).Base.Name + "\", " + cNumericFieldNames(7) +
                                     " is less than 2 deg C. Freezing could occur.");
                }
            }

            if (!lAlphaFieldBlanks(8)) {
                ConstCOPChiller(ChillerNum).Base.BasinHeaterSchedulePtr = GetScheduleIndex(cAlphaArgs(8));
                if (ConstCOPChiller(ChillerNum).Base.BasinHeaterSchedulePtr == 0) {
                    ShowWarningError(cCurrentModuleObject + ", \"" + ConstCOPChiller(ChillerNum).Base.Name + "\" TRIM(cAlphaFieldNames(8)) \"" +
                                     cAlphaArgs(8) + "\" was not found. Basin heater operation will not be modeled and the simulation continues");
                }
            }
        }

        if (ErrorsFound) {
            ShowFatalError("Errors found in processing input for " + cCurrentModuleObject);
        }

        for (ChillerNum = 1; ChillerNum <= NumConstCOPChillers; ++ChillerNum) {
            SetupOutputVariable("Chiller Electric Power",
                                OutputProcessor::Unit::W,
                                ConstCOPChillerReport(ChillerNum).Base.Power,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Electric Energy",
                                OutputProcessor::Unit::J,
                                ConstCOPChillerReport(ChillerNum).Base.Energy,
                                "System",
                                "Sum",
                                ConstCOPChiller(ChillerNum).Base.Name,
                                _,
                                "ELECTRICITY",
                                "Cooling",
                                _,
                                "Plant");

            SetupOutputVariable("Chiller Evaporator Cooling Rate",
                                OutputProcessor::Unit::W,
                                ConstCOPChillerReport(ChillerNum).Base.QEvap,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Cooling Energy",
                                OutputProcessor::Unit::J,
                                ConstCOPChillerReport(ChillerNum).Base.EvapEnergy,
                                "System",
                                "Sum",
                                ConstCOPChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "CHILLERS",
                                _,
                                "Plant");
            SetupOutputVariable("Chiller Evaporator Inlet Temperature",
                                OutputProcessor::Unit::C,
                                ConstCOPChillerReport(ChillerNum).Base.EvapInletTemp,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Outlet Temperature",
                                OutputProcessor::Unit::C,
                                ConstCOPChillerReport(ChillerNum).Base.EvapOutletTemp,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Evaporator Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                ConstCOPChillerReport(ChillerNum).Base.Evapmdot,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller COP",
                                OutputProcessor::Unit::W_W,
                                ConstCOPChillerReport(ChillerNum).ActualCOP,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);

            SetupOutputVariable("Chiller Condenser Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                ConstCOPChillerReport(ChillerNum).Base.QCond,
                                "System",
                                "Average",
                                ConstCOPChiller(ChillerNum).Base.Name);
            SetupOutputVariable("Chiller Condenser Heat Transfer Energy",
                                OutputProcessor::Unit::J,
                                ConstCOPChillerReport(ChillerNum).Base.CondEnergy,
                                "System",
                                "Sum",
                                ConstCOPChiller(ChillerNum).Base.Name,
                                _,
                                "ENERGYTRANSFER",
                                "HEATREJECTION",
                                _,
                                "Plant");

            // Condenser mass flow and outlet temp are valid for water cooled
            if (ConstCOPChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ConstCOPChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    ConstCOPChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Outlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ConstCOPChillerReport(ChillerNum).Base.CondOutletTemp,
                                    "System",
                                    "Average",
                                    ConstCOPChiller(ChillerNum).Base.Name);
                SetupOutputVariable("Chiller Condenser Mass Flow Rate",
                                    OutputProcessor::Unit::kg_s,
                                    ConstCOPChillerReport(ChillerNum).Base.Condmdot,
                                    "System",
                                    "Average",
                                    ConstCOPChiller(ChillerNum).Base.Name);
            } else if (ConstCOPChiller(ChillerNum).Base.CondenserType == AirCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ConstCOPChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    ConstCOPChiller(ChillerNum).Base.Name);
            } else if (ConstCOPChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                SetupOutputVariable("Chiller Condenser Inlet Temperature",
                                    OutputProcessor::Unit::C,
                                    ConstCOPChillerReport(ChillerNum).Base.CondInletTemp,
                                    "System",
                                    "Average",
                                    ConstCOPChiller(ChillerNum).Base.Name);
                if (ConstCOPChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff > 0.0) {
                    SetupOutputVariable("Chiller Basin Heater Electric Power",
                                        OutputProcessor::Unit::W,
                                        ConstCOPChillerReport(ChillerNum).Base.BasinHeaterPower,
                                        "System",
                                        "Average",
                                        ConstCOPChiller(ChillerNum).Base.Name);
                    SetupOutputVariable("Chiller Basin Heater Electric Energy",
                                        OutputProcessor::Unit::J,
                                        ConstCOPChillerReport(ChillerNum).Base.BasinHeaterConsumption,
                                        "System",
                                        "Sum",
                                        ConstCOPChiller(ChillerNum).Base.Name,
                                        _,
                                        "Electric",
                                        "CHILLERS",
                                        _,
                                        "Plant");
                }
            }
            if (AnyEnergyManagementSystemInModel) {
                SetupEMSInternalVariable(
                    "Chiller Nominal Capacity", ConstCOPChiller(ChillerNum).Base.Name, "[W]", ConstCOPChiller(ChillerNum).Base.NomCap);
            }
        }
    }

    void InitElectricChiller(int const ChillNum, // number of the current electric chiller being simulated
                             bool const RunFlag, // TRUE when chiller operating
                             Real64 const MyLoad)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   April 2002
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for initializations of the Electric Chiller components

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("InitElectricChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int CondInletNode;  // node number of water inlet node to the condenser
        int CondOutletNode; // node number of water outlet node from the condenser
        int EvapInletNode;
        int EvapOutletNode;
        bool errFlag;
        Real64 rho;                   // local fluid density
        Real64 mdot;                  // local mass flow rate
        Real64 mdotCond;              // local mass flow rate for condenser
        Real64 THeatRecSetPoint(0.0); // tests set point node for proper set point value

        int InletNode;
        int OutletNode;
        int LoopNum;
        int LoopSideNum;
        int BranchIndex;
        int CompIndex;
        bool FatalError;

        CondInletNode = ElectricChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = ElectricChiller(ChillNum).Base.CondOutletNodeNum;
        EvapInletNode = ElectricChiller(ChillNum).Base.EvapInletNodeNum;
        EvapOutletNode = ElectricChiller(ChillNum).Base.EvapOutletNodeNum;

        // Init more variables
        if (ElectricChiller(ChillNum).Base.MyFlag) {
            // Locate the chillers on the plant loops for later usage
            errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(ElectricChiller(ChillNum).Base.Name,
                                    TypeOf_Chiller_Electric,
                                    ElectricChiller(ChillNum).Base.CWLoopNum,
                                    ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                    ElectricChiller(ChillNum).Base.CWBranchNum,
                                    ElectricChiller(ChillNum).Base.CWCompNum,
                                    errFlag,
                                    ElectricChiller(ChillNum).TempLowLimitEvapOut,
                                    _,
                                    _,
                                    ElectricChiller(ChillNum).Base.EvapInletNodeNum,
                                    _);
            if (ElectricChiller(ChillNum).Base.CondenserType != AirCooled && ElectricChiller(ChillNum).Base.CondenserType != EvapCooled) {
                PlantUtilities::ScanPlantLoopsForObject(ElectricChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_Electric,
                                        ElectricChiller(ChillNum).Base.CDLoopNum,
                                        ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                        ElectricChiller(ChillNum).Base.CDBranchNum,
                                        ElectricChiller(ChillNum).Base.CDCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        ElectricChiller(ChillNum).Base.CondInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(ElectricChiller(ChillNum).Base.CWLoopNum,
                                              ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                              ElectricChiller(ChillNum).Base.CDLoopNum,
                                              ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                              TypeOf_Chiller_Electric,
                                              true);
            }
            if (ElectricChiller(ChillNum).HeatRecActive) {
                PlantUtilities::ScanPlantLoopsForObject(ElectricChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_Electric,
                                        ElectricChiller(ChillNum).HRLoopNum,
                                        ElectricChiller(ChillNum).HRLoopSideNum,
                                        ElectricChiller(ChillNum).HRBranchNum,
                                        ElectricChiller(ChillNum).HRCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        ElectricChiller(ChillNum).HeatRecInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(ElectricChiller(ChillNum).Base.CWLoopNum,
                                              ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                              ElectricChiller(ChillNum).HRLoopNum,
                                              ElectricChiller(ChillNum).HRLoopSideNum,
                                              TypeOf_Chiller_Electric,
                                              true);
            }

            if (ElectricChiller(ChillNum).Base.CondenserType != AirCooled && ElectricChiller(ChillNum).Base.CondenserType != EvapCooled &&
                ElectricChiller(ChillNum).HeatRecActive) {
                PlantUtilities::InterConnectTwoPlantLoopSides(ElectricChiller(ChillNum).Base.CDLoopNum,
                                              ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                              ElectricChiller(ChillNum).HRLoopNum,
                                              ElectricChiller(ChillNum).HRLoopSideNum,
                                              TypeOf_Chiller_Electric,
                                              false);
            }

            if (errFlag) {
                ShowFatalError("InitElectricChiller: Program terminated due to previous condition(s).");
            }

            if (ElectricChiller(ChillNum).Base.FlowMode == ConstantFlow) {
                // reset flow priority
                DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(ElectricChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(ElectricChiller(ChillNum).Base.CWBranchNum)
                    .Comp(ElectricChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
            }

            if (ElectricChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) {
                // reset flow priority
                DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(ElectricChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(ElectricChiller(ChillNum).Base.CWBranchNum)
                    .Comp(ElectricChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;

                // check if setpoint on outlet node
                if ((Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint == SensedNodeFlagValue) &&
                    (Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi == SensedNodeFlagValue)) {
                    if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                        if (!ElectricChiller(ChillNum).Base.ModulatedFlowErrDone) {
                            ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                             ElectricChiller(ChillNum).Base.Name);
                            ShowContinueError(
                                "  A temperature setpoint is needed at the outlet node of a chiller in variable flow mode, use a SetpointManager");
                            ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                            ElectricChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                        }
                    } else {
                        // need call to EMS to check node
                        FatalError = false; // but not really fatal yet, but should be.
                        EMSManager::CheckIfNodeSetPointManagedByEMS(ElectricChiller(ChillNum).Base.EvapOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                        if (FatalError) {
                            if (!ElectricChiller(ChillNum).Base.ModulatedFlowErrDone) {
                                ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                                 ElectricChiller(ChillNum).Base.Name);
                                ShowContinueError(
                                    "  A temperature setpoint is needed at the outlet node of a chiller evaporator in variable flow mode");
                                ShowContinueError("  use a Setpoint Manager to establish a setpoint at the chiller evaporator outlet node ");
                                ShowContinueError("  or use an EMS actuator to establish a setpoint at the outlet node ");
                                ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                                ElectricChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                            }
                        }
                    }
                    ElectricChiller(ChillNum).Base.ModulatedFlowSetToLoop = true;
                    {
                        auto const SELECT_CASE_var(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).LoopDemandCalcScheme);
                        if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                            Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                                Node(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                        } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                            Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                                Node(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
                        }
                    }
                }
            }
            ElectricChiller(ChillNum).Base.MyFlag = false;
        }

        if (ElectricChiller(ChillNum).Base.MyEnvrnFlag && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidName,
                                   DataGlobals::CWInitConvTemp,
                                   DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);

            ElectricChiller(ChillNum).Base.EvapMassFlowRateMax = rho * ElectricChiller(ChillNum).Base.EvapVolFlowRate;
            PlantUtilities::InitComponentNodes(0.0,
                               ElectricChiller(ChillNum).Base.EvapMassFlowRateMax,
                               EvapInletNode,
                               EvapOutletNode,
                               ElectricChiller(ChillNum).Base.CWLoopNum,
                               ElectricChiller(ChillNum).Base.CWLoopSideNum,
                               ElectricChiller(ChillNum).Base.CWBranchNum,
                               ElectricChiller(ChillNum).Base.CWCompNum);

            // init maximum available condenser flow rate
            if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {

                Node(CondInletNode).Temp = ElectricChiller(ChillNum).TempDesCondIn; // DSU? old behavior, still want?

                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);

                ElectricChiller(ChillNum).Base.CondMassFlowRateMax = rho * ElectricChiller(ChillNum).Base.CondVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   ElectricChiller(ChillNum).Base.CondMassFlowRateMax,
                                   CondInletNode,
                                   CondOutletNode,
                                   ElectricChiller(ChillNum).Base.CDLoopNum,
                                   ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                   ElectricChiller(ChillNum).Base.CDBranchNum,
                                   ElectricChiller(ChillNum).Base.CDCompNum);
            } else { // air or evap-air

                rho = Psychrometrics::PsyRhoAirFnPbTdbW(DataEnvironment::StdBaroPress, ElectricChiller(ChillNum).TempDesCondIn, 0.0, RoutineName);
                ElectricChiller(ChillNum).Base.CondMassFlowRateMax = rho * ElectricChiller(ChillNum).Base.CondVolFlowRate;

                Node(CondInletNode).MassFlowRate = ElectricChiller(ChillNum).Base.CondMassFlowRateMax;
                Node(CondOutletNode).MassFlowRate = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMaxAvail = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondOutletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMinAvail = 0.0;
                Node(CondInletNode).MassFlowRateMin = 0.0;
                Node(CondOutletNode).MassFlowRateMinAvail = 0.0;
                Node(CondOutletNode).MassFlowRateMin = 0.0;
            }

            if (ElectricChiller(ChillNum).HeatRecActive) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).FluidName,
                                       DataGlobals::HWInitConvTemp,
                                       DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).FluidIndex,
                                       RoutineName);
                ElectricChiller(ChillNum).DesignHeatRecMassFlowRate = rho * ElectricChiller(ChillNum).DesignHeatRecVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   ElectricChiller(ChillNum).DesignHeatRecMassFlowRate,
                                   ElectricChiller(ChillNum).HeatRecInletNodeNum,
                                   ElectricChiller(ChillNum).HeatRecOutletNodeNum,
                                   ElectricChiller(ChillNum).HRLoopNum,
                                   ElectricChiller(ChillNum).HRLoopSideNum,
                                   ElectricChiller(ChillNum).HRBranchNum,
                                   ElectricChiller(ChillNum).HRCompNum);
                ElectricChiller(ChillNum).HeatRecMaxCapacityLimit =
                    ElectricChiller(ChillNum).HeatRecCapacityFraction *
                    (ElectricChiller(ChillNum).Base.NomCap + ElectricChiller(ChillNum).Base.NomCap / ElectricChiller(ChillNum).Base.COP);

                if (ElectricChiller(ChillNum).HeatRecSetPointNodeNum > 0) {
                    {
                        auto const SELECT_CASE_var(DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).LoopDemandCalcScheme);
                        if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                            THeatRecSetPoint = Node(ElectricChiller(ChillNum).HeatRecSetPointNodeNum).TempSetPoint;
                        } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                            THeatRecSetPoint = Node(ElectricChiller(ChillNum).HeatRecSetPointNodeNum).TempSetPointHi;
                        }
                    }
                    if (THeatRecSetPoint == SensedNodeFlagValue) {
                        if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                            if (!ElectricChiller(ChillNum).Base.HRSPErrDone) {
                                ShowWarningError("Missing heat recovery temperature setpoint for chiller named " +
                                                 ElectricChiller(ChillNum).Base.Name);
                                ShowContinueError("  A temperature setpoint is needed at the heat recovery leaving temperature setpoint node "
                                                  "specified, use a SetpointManager");
                                ShowContinueError("  The overall loop setpoint will be assumed for heat recovery. The simulation continues ...");
                                ElectricChiller(ChillNum).HeatRecSetPointNodeNum = DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).TempSetPointNodeNum;
                                ElectricChiller(ChillNum).Base.HRSPErrDone = true;
                            }
                        } else {
                            // need call to EMS to check node
                            FatalError = false; // but not really fatal yet, but should be.
                            EMSManager::CheckIfNodeSetPointManagedByEMS(ElectricChiller(ChillNum).Base.EvapOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                            if (FatalError) {
                                if (!ElectricChiller(ChillNum).Base.HRSPErrDone) {
                                    ShowWarningError("Missing heat recovery temperature setpoint for chiller named " +
                                                     ElectricChiller(ChillNum).Base.Name);
                                    ShowContinueError("  A temperature setpoint is needed at the heat recovery leaving temperature setpoint node "
                                                      "specified, use a SetpointManager to establish a setpoint");
                                    ShowContinueError("  or use an EMS actuator to establish a setpoint at this node ");
                                    ShowContinueError("  The overall loop setpoint will be assumed for heat recovery. The simulation continues ...");
                                    ElectricChiller(ChillNum).HeatRecSetPointNodeNum =
                                        DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).TempSetPointNodeNum;
                                    ElectricChiller(ChillNum).Base.HRSPErrDone = true;
                                }
                            }
                        } // IF (.NOT. AnyEnergyManagementSystemInModel) THEN
                    }     // IF(THeatRecSetpoint == SensedNodeFlagValue)THEN
                }         // IF(ElectricChiller(ChillNum)%HeatRecSetpointNodeNum > 0)THEN
            }             // IF (ElectricChiller(ChillNum)%HeatRecActive) THEN

            ElectricChiller(ChillNum).Base.MyEnvrnFlag = false;
        }
        if (!DataGlobals::BeginEnvrnFlag) {
            ElectricChiller(ChillNum).Base.MyEnvrnFlag = true;
        }

        if ((ElectricChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) && (ElectricChiller(ChillNum).Base.ModulatedFlowSetToLoop)) {
            // fix for clumsy old input that worked because loop setpoint was spread.
            //  could be removed with transition, testing , model change, period of being obsolete.
            {
                auto const SELECT_CASE_var(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).LoopDemandCalcScheme);
                if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                    Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                        Node(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                    Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                        Node(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
                }
            }
        }

        if ((MyLoad < 0.0) && RunFlag) {
            // request full then take what can get
            mdot = ElectricChiller(ChillNum).Base.EvapMassFlowRateMax;
            mdotCond = ElectricChiller(ChillNum).Base.CondMassFlowRateMax;
        } else {
            mdot = 0.0;
            mdotCond = 0.0;
        }

        PlantUtilities::SetComponentFlowRate(mdot,
                             EvapInletNode,
                             EvapOutletNode,
                             ElectricChiller(ChillNum).Base.CWLoopNum,
                             ElectricChiller(ChillNum).Base.CWLoopSideNum,
                             ElectricChiller(ChillNum).Base.CWBranchNum,
                             ElectricChiller(ChillNum).Base.CWCompNum);
        if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PlantUtilities::SetComponentFlowRate(mdotCond,
                                 CondInletNode,
                                 CondOutletNode,
                                 ElectricChiller(ChillNum).Base.CDLoopNum,
                                 ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                 ElectricChiller(ChillNum).Base.CDBranchNum,
                                 ElectricChiller(ChillNum).Base.CDCompNum);
        }

        // Initialize heat recovery flow rates at node
        if (ElectricChiller(ChillNum).HeatRecActive) {

            InletNode = ElectricChiller(ChillNum).HeatRecInletNodeNum;
            OutletNode = ElectricChiller(ChillNum).HeatRecOutletNodeNum;
            LoopNum = ElectricChiller(ChillNum).HRLoopNum;
            LoopSideNum = ElectricChiller(ChillNum).HRLoopSideNum;
            BranchIndex = ElectricChiller(ChillNum).HRBranchNum;
            CompIndex = ElectricChiller(ChillNum).HRCompNum;

            if (RunFlag) {
                mdot = ElectricChiller(ChillNum).DesignHeatRecMassFlowRate;
            } else {
                mdot = 0.0;
            }

            PlantUtilities::SetComponentFlowRate(mdot, InletNode, OutletNode, LoopNum, LoopSideNum, BranchIndex, CompIndex);
        }

        if (ElectricChiller(ChillNum).Base.CondenserType == EvapCooled) {
            modBasinHeaterPower = 0.0;
        }
    }

    void InitEngineDrivenChiller(int const ChillNum, // number of the current engine driven chiller being simulated
                                 bool const RunFlag, // TRUE when chiller operating
                                 Real64 const MyLoad)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   June 2002
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for initializations of the Engine Driven Chiller components

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("InitEngineDrivenChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int CondInletNode; // node number of water inlet node to the condenser
        int CondOutletNode;
        int EvapInletNode;
        int EvapOutletNode;
        Real64 rho;      // local fluid density
        Real64 mdot;     // local mass flow rate
        Real64 mdotCond; // local mass flow rate for condenser
        bool errFlag;
        int InletNode;
        int OutletNode;
        int LoopNum;
        int LoopSideNum;
        int BranchIndex;
        int CompIndex;
        bool FatalError;

        // Load inputs to local structure
        CondInletNode = EngineDrivenChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = EngineDrivenChiller(ChillNum).Base.CondOutletNodeNum;
        EvapInletNode = EngineDrivenChiller(ChillNum).Base.EvapInletNodeNum;
        EvapOutletNode = EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum;

        // Init more variables
        if (EngineDrivenChiller(ChillNum).Base.MyFlag) {
            // Locate the chillers on the plant loops for later usage
            errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(EngineDrivenChiller(ChillNum).Base.Name,
                                    TypeOf_Chiller_EngineDriven,
                                    EngineDrivenChiller(ChillNum).Base.CWLoopNum,
                                    EngineDrivenChiller(ChillNum).Base.CWLoopSideNum,
                                    EngineDrivenChiller(ChillNum).Base.CWBranchNum,
                                    EngineDrivenChiller(ChillNum).Base.CWCompNum,
                                    errFlag,
                                    EngineDrivenChiller(ChillNum).TempLowLimitEvapOut,
                                    _,
                                    _,
                                    EngineDrivenChiller(ChillNum).Base.EvapInletNodeNum,
                                    _);
            if (EngineDrivenChiller(ChillNum).Base.CondenserType != AirCooled && EngineDrivenChiller(ChillNum).Base.CondenserType != EvapCooled) {
                PlantUtilities::ScanPlantLoopsForObject(EngineDrivenChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_EngineDriven,
                                        EngineDrivenChiller(ChillNum).Base.CDLoopNum,
                                        EngineDrivenChiller(ChillNum).Base.CDLoopSideNum,
                                        EngineDrivenChiller(ChillNum).Base.CDBranchNum,
                                        EngineDrivenChiller(ChillNum).Base.CDCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        EngineDrivenChiller(ChillNum).Base.CondInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(EngineDrivenChiller(ChillNum).Base.CWLoopNum,
                                              EngineDrivenChiller(ChillNum).Base.CWLoopSideNum,
                                              EngineDrivenChiller(ChillNum).Base.CDLoopNum,
                                              EngineDrivenChiller(ChillNum).Base.CDLoopSideNum,
                                              TypeOf_Chiller_EngineDriven,
                                              true);
            }
            if (EngineDrivenChiller(ChillNum).HeatRecActive) {
                PlantUtilities::ScanPlantLoopsForObject(EngineDrivenChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_EngineDriven,
                                        EngineDrivenChiller(ChillNum).HRLoopNum,
                                        EngineDrivenChiller(ChillNum).HRLoopSideNum,
                                        EngineDrivenChiller(ChillNum).HRBranchNum,
                                        EngineDrivenChiller(ChillNum).HRCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        EngineDrivenChiller(ChillNum).HeatRecInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(EngineDrivenChiller(ChillNum).Base.CWLoopNum,
                                              EngineDrivenChiller(ChillNum).Base.CWLoopSideNum,
                                              EngineDrivenChiller(ChillNum).HRLoopNum,
                                              EngineDrivenChiller(ChillNum).HRLoopSideNum,
                                              TypeOf_Chiller_EngineDriven,
                                              true);
            }

            if (EngineDrivenChiller(ChillNum).Base.CondenserType != AirCooled && EngineDrivenChiller(ChillNum).Base.CondenserType != EvapCooled &&
                EngineDrivenChiller(ChillNum).HeatRecActive) {
                PlantUtilities::InterConnectTwoPlantLoopSides(EngineDrivenChiller(ChillNum).Base.CDLoopNum,
                                              EngineDrivenChiller(ChillNum).Base.CDLoopSideNum,
                                              EngineDrivenChiller(ChillNum).HRLoopNum,
                                              EngineDrivenChiller(ChillNum).HRLoopSideNum,
                                              TypeOf_Chiller_EngineDriven,
                                              false);
            }
            if (errFlag) {
                ShowFatalError("InitEngineDrivenChiller: Program terminated due to previous condition(s).");
            }

            if (EngineDrivenChiller(ChillNum).Base.FlowMode == ConstantFlow) {
                // reset flow priority
                DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(EngineDrivenChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(EngineDrivenChiller(ChillNum).Base.CWBranchNum)
                    .Comp(EngineDrivenChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
            }

            if (EngineDrivenChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) {
                // reset flow priority
                DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(EngineDrivenChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(EngineDrivenChiller(ChillNum).Base.CWBranchNum)
                    .Comp(EngineDrivenChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
                // check if setpoint on outlet node
                if ((Node(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint == SensedNodeFlagValue) &&
                    (Node(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi == SensedNodeFlagValue)) {
                    if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                        if (!EngineDrivenChiller(ChillNum).Base.ModulatedFlowErrDone) {
                            ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                             EngineDrivenChiller(ChillNum).Base.Name);
                            ShowContinueError(
                                "  A temperature setpoint is needed at the outlet node of a chiller in variable flow mode, use a SetpointManager");
                            ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                            EngineDrivenChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                        }
                    } else {
                        // need call to EMS to check node
                        FatalError = false; // but not really fatal yet, but should be.
                        EMSManager::CheckIfNodeSetPointManagedByEMS(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                        if (FatalError) {
                            if (!EngineDrivenChiller(ChillNum).Base.ModulatedFlowErrDone) {
                                ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                                 EngineDrivenChiller(ChillNum).Base.Name);
                                ShowContinueError(
                                    "  A temperature setpoint is needed at the outlet node of a chiller evaporator in variable flow mode");
                                ShowContinueError("  use a Setpoint Manager to establish a setpoint at the chiller evaporator outlet node ");
                                ShowContinueError("  or use an EMS actuator to establish a setpoint at the outlet node ");
                                ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                                EngineDrivenChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                            }
                        }
                    }
                    EngineDrivenChiller(ChillNum).Base.ModulatedFlowSetToLoop = true;
                    Node(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                        Node(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                    Node(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                        Node(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
                }
            }

            EngineDrivenChiller(ChillNum).Base.MyFlag = false;
        }

        // Initialize critical Demand Side Variables
        //  IF((MyEnvrnFlag(ChillNum) .and. BeginEnvrnFlag) &
        //     .OR. (Node(CondInletNode)%MassFlowrate <= 0.0 .AND. RunFlag)) THEN
        if (EngineDrivenChiller(ChillNum).Base.MyEnvrnFlag && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).FluidName,
                                   DataGlobals::CWInitConvTemp,
                                   DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);

            EngineDrivenChiller(ChillNum).Base.EvapMassFlowRateMax = rho * EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate;
            PlantUtilities::InitComponentNodes(0.0,
                               EngineDrivenChiller(ChillNum).Base.EvapMassFlowRateMax,
                               EvapInletNode,
                               EvapOutletNode,
                               EngineDrivenChiller(ChillNum).Base.CWLoopNum,
                               EngineDrivenChiller(ChillNum).Base.CWLoopSideNum,
                               EngineDrivenChiller(ChillNum).Base.CWBranchNum,
                               EngineDrivenChiller(ChillNum).Base.CWCompNum);

            // init maximum available condenser flow rate

            if (EngineDrivenChiller(ChillNum).Base.CondenserType == WaterCooled) {

                Node(CondInletNode).Temp = EngineDrivenChiller(ChillNum).TempDesCondIn;

                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);

                EngineDrivenChiller(ChillNum).Base.CondMassFlowRateMax = rho * EngineDrivenChiller(ChillNum).Base.CondVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   EngineDrivenChiller(ChillNum).Base.CondMassFlowRateMax,
                                   CondInletNode,
                                   CondOutletNode,
                                   EngineDrivenChiller(ChillNum).Base.CDLoopNum,
                                   EngineDrivenChiller(ChillNum).Base.CDLoopSideNum,
                                   EngineDrivenChiller(ChillNum).Base.CDBranchNum,
                                   EngineDrivenChiller(ChillNum).Base.CDCompNum);
            } else { // air or evap-air
                Node(CondInletNode).MassFlowRate = EngineDrivenChiller(ChillNum).Base.CondVolFlowRate *
                    Psychrometrics::PsyRhoAirFnPbTdbW(DataEnvironment::StdBaroPress, EngineDrivenChiller(ChillNum).TempDesCondIn, 0.0, RoutineName);

                Node(CondOutletNode).MassFlowRate = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMaxAvail = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondOutletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMinAvail = 0.0;
                Node(CondInletNode).MassFlowRateMin = 0.0;
                Node(CondOutletNode).MassFlowRateMinAvail = 0.0;
                Node(CondOutletNode).MassFlowRateMin = 0.0;
            }

            if (EngineDrivenChiller(ChillNum).HeatRecActive) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).HRLoopNum).FluidName,
                                       DataGlobals::HWInitConvTemp,
                                       DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).HRLoopNum).FluidIndex,
                                       RoutineName);
                EngineDrivenChiller(ChillNum).DesignHeatRecMassFlowRate = rho * EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   EngineDrivenChiller(ChillNum).DesignHeatRecMassFlowRate,
                                   EngineDrivenChiller(ChillNum).HeatRecInletNodeNum,
                                   EngineDrivenChiller(ChillNum).HeatRecOutletNodeNum,
                                   EngineDrivenChiller(ChillNum).HRLoopNum,
                                   EngineDrivenChiller(ChillNum).HRLoopSideNum,
                                   EngineDrivenChiller(ChillNum).HRBranchNum,
                                   EngineDrivenChiller(ChillNum).HRCompNum);
            }

            EngineDrivenChiller(ChillNum).Base.MyEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            EngineDrivenChiller(ChillNum).Base.MyEnvrnFlag = true;
        }

        if ((EngineDrivenChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) &&
            (EngineDrivenChiller(ChillNum).Base.ModulatedFlowSetToLoop)) {
            // fix for clumsy old input that worked because loop setpoint was spread.
            //  could be removed with transition, testing , model change, period of being obsolete.
            Node(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                Node(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
            Node(EngineDrivenChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                Node(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
        }

        if ((std::abs(MyLoad) > 0.0) && RunFlag) {
            mdot = EngineDrivenChiller(ChillNum).Base.EvapMassFlowRateMax;
            mdotCond = EngineDrivenChiller(ChillNum).Base.CondMassFlowRateMax;
        } else {
            mdot = 0.0;
            mdotCond = 0.0;
        }

        PlantUtilities::SetComponentFlowRate(mdot,
                             EvapInletNode,
                             EvapOutletNode,
                             EngineDrivenChiller(ChillNum).Base.CWLoopNum,
                             EngineDrivenChiller(ChillNum).Base.CWLoopSideNum,
                             EngineDrivenChiller(ChillNum).Base.CWBranchNum,
                             EngineDrivenChiller(ChillNum).Base.CWCompNum);
        if (EngineDrivenChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PlantUtilities::SetComponentFlowRate(mdotCond,
                                 CondInletNode,
                                 CondOutletNode,
                                 EngineDrivenChiller(ChillNum).Base.CDLoopNum,
                                 EngineDrivenChiller(ChillNum).Base.CDLoopSideNum,
                                 EngineDrivenChiller(ChillNum).Base.CDBranchNum,
                                 EngineDrivenChiller(ChillNum).Base.CDCompNum);
        }

        // Initialize heat recovery flow rates at node
        if (EngineDrivenChiller(ChillNum).HeatRecActive) {
            InletNode = EngineDrivenChiller(ChillNum).HeatRecInletNodeNum;
            OutletNode = EngineDrivenChiller(ChillNum).HeatRecOutletNodeNum;
            LoopNum = EngineDrivenChiller(ChillNum).HRLoopNum;
            LoopSideNum = EngineDrivenChiller(ChillNum).HRLoopSideNum;
            BranchIndex = EngineDrivenChiller(ChillNum).HRBranchNum;
            CompIndex = EngineDrivenChiller(ChillNum).HRCompNum;

            if (RunFlag) {
                mdot = EngineDrivenChiller(ChillNum).DesignHeatRecMassFlowRate;
            } else {
                mdot = 0.0;
            }

            PlantUtilities::SetComponentFlowRate(mdot, InletNode, OutletNode, LoopNum, LoopSideNum, BranchIndex, CompIndex);
        }
        if (EngineDrivenChiller(ChillNum).Base.CondenserType == EvapCooled) {
            modBasinHeaterPower = 0.0;
        }
    }

    void InitGTChiller(int const ChillNum, // number of the current engine driven chiller being simulated
                       bool const RunFlag, // TRUE when chiller operating
                       Real64 const MyLoad)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   November 2003
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for initializations of the Gas Turbine Chiller components

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("InitGTChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int CondInletNode;  // node number of water inlet node to the condenser
        int CondOutletNode; // node number of water outlet node from the condenser
        int EvapInletNode;
        int EvapOutletNode;
        Real64 rho;      // local fluid density
        Real64 mdot;     // local mass flow rate
        Real64 mdotCond; // local mass flow rate for condenser
        int InletNode;
        int OutletNode;
        int LoopNum;
        int LoopSideNum;
        int BranchIndex;
        int CompIndex;
        bool FatalError;
        bool errFlag;
        // FLOW:

        CondInletNode = GTChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = GTChiller(ChillNum).Base.CondOutletNodeNum;
        EvapInletNode = GTChiller(ChillNum).Base.EvapInletNodeNum;
        EvapOutletNode = GTChiller(ChillNum).Base.EvapOutletNodeNum;

        // Init more variables
        if (GTChiller(ChillNum).Base.MyFlag) {
            // Locate the chillers on the plant loops for later usage
            errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(GTChiller(ChillNum).Base.Name,
                                    TypeOf_Chiller_CombTurbine,
                                    GTChiller(ChillNum).Base.CWLoopNum,
                                    GTChiller(ChillNum).Base.CWLoopSideNum,
                                    GTChiller(ChillNum).Base.CWBranchNum,
                                    GTChiller(ChillNum).Base.CWCompNum,
                                    errFlag,
                                    GTChiller(ChillNum).TempLowLimitEvapOut,
                                    _,
                                    _,
                                    GTChiller(ChillNum).Base.EvapInletNodeNum,
                                    _);
            if (GTChiller(ChillNum).Base.CondenserType != AirCooled && GTChiller(ChillNum).Base.CondenserType != EvapCooled) {
                PlantUtilities::ScanPlantLoopsForObject(GTChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_CombTurbine,
                                        GTChiller(ChillNum).Base.CDLoopNum,
                                        GTChiller(ChillNum).Base.CDLoopSideNum,
                                        GTChiller(ChillNum).Base.CDBranchNum,
                                        GTChiller(ChillNum).Base.CDCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        GTChiller(ChillNum).Base.CondInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(GTChiller(ChillNum).Base.CWLoopNum,
                                              GTChiller(ChillNum).Base.CWLoopSideNum,
                                              GTChiller(ChillNum).Base.CDLoopNum,
                                              GTChiller(ChillNum).Base.CDLoopSideNum,
                                              TypeOf_Chiller_CombTurbine,
                                              true);
            }
            if (GTChiller(ChillNum).HeatRecActive) {
                PlantUtilities::ScanPlantLoopsForObject(GTChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_CombTurbine,
                                        GTChiller(ChillNum).HRLoopNum,
                                        GTChiller(ChillNum).HRLoopSideNum,
                                        GTChiller(ChillNum).HRBranchNum,
                                        GTChiller(ChillNum).HRCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        GTChiller(ChillNum).HeatRecInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(GTChiller(ChillNum).Base.CWLoopNum,
                                              GTChiller(ChillNum).Base.CWLoopSideNum,
                                              GTChiller(ChillNum).HRLoopNum,
                                              GTChiller(ChillNum).HRLoopSideNum,
                                              TypeOf_Chiller_CombTurbine,
                                              true);
            }

            if (GTChiller(ChillNum).Base.CondenserType != AirCooled && GTChiller(ChillNum).Base.CondenserType != EvapCooled &&
                GTChiller(ChillNum).HeatRecActive) {
                PlantUtilities::InterConnectTwoPlantLoopSides(GTChiller(ChillNum).Base.CDLoopNum,
                                              GTChiller(ChillNum).Base.CDLoopSideNum,
                                              GTChiller(ChillNum).HRLoopNum,
                                              GTChiller(ChillNum).HRLoopSideNum,
                                              TypeOf_Chiller_CombTurbine,
                                              false);
            }
            if (errFlag) {
                ShowFatalError("InitGTChiller: Program terminated due to previous condition(s).");
            }

            if (GTChiller(ChillNum).Base.FlowMode == ConstantFlow) {
                // reset flow priority
                DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(GTChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(GTChiller(ChillNum).Base.CWBranchNum)
                    .Comp(GTChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
            }

            if (GTChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) {
                // reset flow priority
                DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(GTChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(GTChiller(ChillNum).Base.CWBranchNum)
                    .Comp(GTChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;

                // check if setpoint on outlet node
                if ((Node(GTChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint == SensedNodeFlagValue) &&
                    (Node(GTChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi == SensedNodeFlagValue)) {
                    if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                        if (!GTChiller(ChillNum).Base.ModulatedFlowErrDone) {
                            ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                             GTChiller(ChillNum).Base.Name);
                            ShowContinueError(
                                "  A temperature setpoint is needed at the outlet node of a chiller in variable flow mode, use a SetpointManager");
                            ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                            GTChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                        }
                    } else {
                        // need call to EMS to check node
                        FatalError = false; // but not really fatal yet, but should be.
                        EMSManager::CheckIfNodeSetPointManagedByEMS(GTChiller(ChillNum).Base.EvapOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                        if (FatalError) {
                            if (!GTChiller(ChillNum).Base.ModulatedFlowErrDone) {
                                ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                                 GTChiller(ChillNum).Base.Name);
                                ShowContinueError(
                                    "  A temperature setpoint is needed at the outlet node of a chiller evaporator in variable flow mode");
                                ShowContinueError("  use a Setpoint Manager to establish a setpoint at the chiller evaporator outlet node ");
                                ShowContinueError("  or use an EMS actuator to establish a setpoint at the outlet node ");
                                ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                                GTChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                            }
                        }
                    }
                    GTChiller(ChillNum).Base.ModulatedFlowSetToLoop = true;
                    Node(GTChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                        Node(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                    Node(GTChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                        Node(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
                }
            }
            GTChiller(ChillNum).Base.MyFlag = false;
        }

        if (GTChiller(ChillNum).Base.MyEnvrnFlag && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).FluidName,
                                   DataGlobals::CWInitConvTemp,
                                   DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);

            GTChiller(ChillNum).Base.EvapMassFlowRateMax = rho * GTChiller(ChillNum).Base.EvapVolFlowRate;
            PlantUtilities::InitComponentNodes(0.0,
                               GTChiller(ChillNum).Base.EvapMassFlowRateMax,
                               EvapInletNode,
                               EvapOutletNode,
                               GTChiller(ChillNum).Base.CWLoopNum,
                               GTChiller(ChillNum).Base.CWLoopSideNum,
                               GTChiller(ChillNum).Base.CWBranchNum,
                               GTChiller(ChillNum).Base.CWCompNum);

            // init maximum available condenser flow rate
            if (GTChiller(ChillNum).Base.CondenserType == WaterCooled) {

                Node(CondInletNode).Temp = GTChiller(ChillNum).TempDesCondIn;

                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);

                GTChiller(ChillNum).Base.CondMassFlowRateMax = rho * GTChiller(ChillNum).Base.CondVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   GTChiller(ChillNum).Base.CondMassFlowRateMax,
                                   CondInletNode,
                                   CondOutletNode,
                                   GTChiller(ChillNum).Base.CDLoopNum,
                                   GTChiller(ChillNum).Base.CDLoopSideNum,
                                   GTChiller(ChillNum).Base.CDBranchNum,
                                   GTChiller(ChillNum).Base.CDCompNum);
            } else { // air or evap-air
                Node(CondInletNode).MassFlowRate =
                    GTChiller(ChillNum).Base.CondVolFlowRate * Psychrometrics::PsyRhoAirFnPbTdbW(DataEnvironment::StdBaroPress, GTChiller(ChillNum).TempDesCondIn, 0.0, RoutineName);

                Node(CondOutletNode).MassFlowRate = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMaxAvail = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondOutletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMinAvail = 0.0;
                Node(CondInletNode).MassFlowRateMin = 0.0;
                Node(CondOutletNode).MassFlowRateMinAvail = 0.0;
                Node(CondOutletNode).MassFlowRateMin = 0.0;
            }

            if (GTChiller(ChillNum).HeatRecActive) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).HRLoopNum).FluidName,
                                       DataGlobals::HWInitConvTemp,
                                       DataPlant::PlantLoop(GTChiller(ChillNum).HRLoopNum).FluidIndex,
                                       RoutineName);
                GTChiller(ChillNum).DesignHeatRecMassFlowRate = rho * GTChiller(ChillNum).DesignHeatRecVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   GTChiller(ChillNum).DesignHeatRecMassFlowRate,
                                   GTChiller(ChillNum).HeatRecInletNodeNum,
                                   GTChiller(ChillNum).HeatRecOutletNodeNum,
                                   GTChiller(ChillNum).HRLoopNum,
                                   GTChiller(ChillNum).HRLoopSideNum,
                                   GTChiller(ChillNum).HRBranchNum,
                                   GTChiller(ChillNum).HRCompNum);
            }

            GTChiller(ChillNum).Base.MyEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            GTChiller(ChillNum).Base.MyEnvrnFlag = true;
        }

        if ((GTChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) && (GTChiller(ChillNum).Base.ModulatedFlowSetToLoop)) {
            // fix for clumsy old input that worked because loop setpoint was spread.
            //  could be removed with transition, testing , model change, period of being obsolete.
            Node(GTChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                Node(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
            Node(GTChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                Node(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
        }

        if ((std::abs(MyLoad) > 0.0) && RunFlag) {
            mdot = GTChiller(ChillNum).Base.EvapMassFlowRateMax;
            mdotCond = GTChiller(ChillNum).Base.CondMassFlowRateMax;
        } else {
            mdot = 0.0;
            mdotCond = 0.0;
        }

        PlantUtilities::SetComponentFlowRate(mdot,
                             EvapInletNode,
                             EvapOutletNode,
                             GTChiller(ChillNum).Base.CWLoopNum,
                             GTChiller(ChillNum).Base.CWLoopSideNum,
                             GTChiller(ChillNum).Base.CWBranchNum,
                             GTChiller(ChillNum).Base.CWCompNum);
        if (GTChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PlantUtilities::SetComponentFlowRate(mdotCond,
                                 CondInletNode,
                                 CondOutletNode,
                                 GTChiller(ChillNum).Base.CDLoopNum,
                                 GTChiller(ChillNum).Base.CDLoopSideNum,
                                 GTChiller(ChillNum).Base.CDBranchNum,
                                 GTChiller(ChillNum).Base.CDCompNum);
        }

        // Initialize heat recovery flow rates at node
        if (GTChiller(ChillNum).HeatRecActive) {

            InletNode = GTChiller(ChillNum).HeatRecInletNodeNum;
            OutletNode = GTChiller(ChillNum).HeatRecOutletNodeNum;
            LoopNum = GTChiller(ChillNum).HRLoopNum;
            LoopSideNum = GTChiller(ChillNum).HRLoopSideNum;
            BranchIndex = GTChiller(ChillNum).HRBranchNum;
            CompIndex = GTChiller(ChillNum).HRCompNum;

            if (RunFlag) {
                mdot = GTChiller(ChillNum).DesignHeatRecMassFlowRate;
            } else {
                mdot = 0.0;
            }

            PlantUtilities::SetComponentFlowRate(mdot, InletNode, OutletNode, LoopNum, LoopSideNum, BranchIndex, CompIndex);
        }
        if (GTChiller(ChillNum).Base.CondenserType == EvapCooled) {
            modBasinHeaterPower = 0.0;
        }
    }

    void InitConstCOPChiller(int const ChillNum, // number of the current electric chiller being simulated
                             bool const RunFlag, // TRUE when chiller operating
                             Real64 const MyLoad)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   September 2010
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for initializations of the Electric Chiller components

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // REFERENCES:
        // Based on InitElectricChiller from Fred Buhl

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("InitConstCOPChiller");
        Real64 const TempDesCondIn(25.0); // Design condenser inlet temp. C

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int CondInletNode;  // node number of water inlet node to the condenser
        int CondOutletNode; // node number of water outlet node from the condenser
        int EvapInletNode;
        int EvapOutletNode;
        Real64 rho;      // local fluid density
        Real64 mdot;     // local mass flow rate
        Real64 mdotCond; // local mass flow rate for condenser
        bool FatalError;
        bool errFlag;

        EvapInletNode = ConstCOPChiller(ChillNum).Base.EvapInletNodeNum;
        EvapOutletNode = ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum;
        CondInletNode = ConstCOPChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = ConstCOPChiller(ChillNum).Base.CondOutletNodeNum;

        // Init more variables
        if (ConstCOPChiller(ChillNum).Base.MyFlag) {
            // Locate the chillers on the plant loops for later usage
            errFlag = false;
            PlantUtilities::ScanPlantLoopsForObject(ConstCOPChiller(ChillNum).Base.Name,
                                    TypeOf_Chiller_ConstCOP,
                                    ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                    ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                    ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                    ConstCOPChiller(ChillNum).Base.CWCompNum,
                                    errFlag,
                                    _,
                                    _,
                                    _,
                                    ConstCOPChiller(ChillNum).Base.EvapInletNodeNum,
                                    _);
            if (ConstCOPChiller(ChillNum).Base.CondenserType != AirCooled && ConstCOPChiller(ChillNum).Base.CondenserType != EvapCooled) {
                PlantUtilities::ScanPlantLoopsForObject(ConstCOPChiller(ChillNum).Base.Name,
                                        TypeOf_Chiller_ConstCOP,
                                        ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                        ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                        ConstCOPChiller(ChillNum).Base.CDBranchNum,
                                        ConstCOPChiller(ChillNum).Base.CDCompNum,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        ConstCOPChiller(ChillNum).Base.CondInletNodeNum,
                                        _);
                PlantUtilities::InterConnectTwoPlantLoopSides(ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                              ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                              ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                              ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                              TypeOf_Chiller_ConstCOP,
                                              true);
            }

            if (errFlag) {
                ShowFatalError("CalcConstCOPChillerModel: Program terminated due to previous condition(s).");
            }
            if (ConstCOPChiller(ChillNum).Base.FlowMode == ConstantFlow) {
                // reset flow priority
                DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(ConstCOPChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(ConstCOPChiller(ChillNum).Base.CWBranchNum)
                    .Comp(ConstCOPChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;
            }

            if (ConstCOPChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) {
                // reset flow priority
                DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum)
                    .LoopSide(ConstCOPChiller(ChillNum).Base.CWLoopSideNum)
                    .Branch(ConstCOPChiller(ChillNum).Base.CWBranchNum)
                    .Comp(ConstCOPChiller(ChillNum).Base.CWCompNum)
                    .FlowPriority = DataPlant::LoopFlowStatus_NeedyIfLoopOn;

                // check if setpoint on outlet node
                if ((Node(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint == SensedNodeFlagValue) &&
                    (Node(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi == SensedNodeFlagValue)) {
                    if (!DataGlobals::AnyEnergyManagementSystemInModel) {
                        if (!ConstCOPChiller(ChillNum).Base.ModulatedFlowErrDone) {
                            ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                             ConstCOPChiller(ChillNum).Base.Name);
                            ShowContinueError(
                                "  A temperature setpoint is needed at the outlet node of a chiller in variable flow mode, use a SetpointManager");
                            ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                            ConstCOPChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                        }
                    } else {
                        // need call to EMS to check node
                        FatalError = false; // but not really fatal yet, but should be.
                        EMSManager::CheckIfNodeSetPointManagedByEMS(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum, EMSManager::iTemperatureSetPoint, FatalError);
                        if (FatalError) {
                            if (!ConstCOPChiller(ChillNum).Base.ModulatedFlowErrDone) {
                                ShowWarningError("Missing temperature setpoint for LeavingSetpointModulated mode chiller named " +
                                                 ConstCOPChiller(ChillNum).Base.Name);
                                ShowContinueError(
                                    "  A temperature setpoint is needed at the outlet node of a chiller evaporator in variable flow mode");
                                ShowContinueError("  use a Setpoint Manager to establish a setpoint at the chiller evaporator outlet node ");
                                ShowContinueError("  or use an EMS actuator to establish a setpoint at the outlet node ");
                                ShowContinueError("  The overall loop setpoint will be assumed for chiller. The simulation continues ... ");
                                ConstCOPChiller(ChillNum).Base.ModulatedFlowErrDone = true;
                            }
                        }
                    }
                    ConstCOPChiller(ChillNum).Base.ModulatedFlowSetToLoop = true;
                    Node(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                        Node(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
                    Node(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                        Node(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
                }
            }
            ConstCOPChiller(ChillNum).Base.MyFlag = false;
        }

        // Initialize critical Demand Side Variables at the beginning of each environment
        if (ConstCOPChiller(ChillNum).Base.MyEnvrnFlag && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidName,
                                   DataGlobals::CWInitConvTemp,
                                   DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);
            ConstCOPChiller(ChillNum).Base.EvapMassFlowRateMax = ConstCOPChiller(ChillNum).Base.EvapVolFlowRate * rho;
            PlantUtilities::InitComponentNodes(0.0,
                               ConstCOPChiller(ChillNum).Base.EvapMassFlowRateMax,
                               EvapInletNode,
                               EvapOutletNode,
                               ConstCOPChiller(ChillNum).Base.CWLoopNum,
                               ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                               ConstCOPChiller(ChillNum).Base.CWBranchNum,
                               ConstCOPChiller(ChillNum).Base.CWCompNum);

            // init maximum available condenser flow rate
            if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {

                Node(CondInletNode).Temp = TempDesCondIn;

                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);

                ConstCOPChiller(ChillNum).Base.CondMassFlowRateMax = rho * ConstCOPChiller(ChillNum).Base.CondVolFlowRate;

                PlantUtilities::InitComponentNodes(0.0,
                                   ConstCOPChiller(ChillNum).Base.CondMassFlowRateMax,
                                   CondInletNode,
                                   CondOutletNode,
                                   ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                   ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                   ConstCOPChiller(ChillNum).Base.CDBranchNum,
                                   ConstCOPChiller(ChillNum).Base.CDCompNum);
            } else { // air or evap-air
                Node(CondInletNode).MassFlowRate =
                    ConstCOPChiller(ChillNum).Base.CondVolFlowRate * Psychrometrics::PsyRhoAirFnPbTdbW(DataEnvironment::StdBaroPress, TempDesCondIn, 0.0, RoutineName);

                Node(CondOutletNode).MassFlowRate = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMaxAvail = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondOutletNode).MassFlowRateMax = Node(CondInletNode).MassFlowRate;
                Node(CondInletNode).MassFlowRateMinAvail = 0.0;
                Node(CondInletNode).MassFlowRateMin = 0.0;
                Node(CondOutletNode).MassFlowRateMinAvail = 0.0;
                Node(CondOutletNode).MassFlowRateMin = 0.0;
            }
            ConstCOPChiller(ChillNum).Base.MyEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            ConstCOPChiller(ChillNum).Base.MyEnvrnFlag = true;
        }
        if ((ConstCOPChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) && (ConstCOPChiller(ChillNum).Base.ModulatedFlowSetToLoop)) {
            // fix for clumsy old input that worked because loop setpoint was spread.
            //  could be removed with transition, testing , model change, period of being obsolete.
            Node(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPoint =
                Node(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPoint;
            Node(ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum).TempSetPointHi =
                Node(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).TempSetPointNodeNum).TempSetPointHi;
        }

        if ((MyLoad < 0.0) && RunFlag) {
            mdot = ConstCOPChiller(ChillNum).Base.EvapMassFlowRateMax;
            mdotCond = ConstCOPChiller(ChillNum).Base.CondMassFlowRateMax;
        } else {
            mdot = 0.0;
            mdotCond = 0.0;
        }

        PlantUtilities::SetComponentFlowRate(mdot,
                             EvapInletNode,
                             EvapOutletNode,
                             ConstCOPChiller(ChillNum).Base.CWLoopNum,
                             ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                             ConstCOPChiller(ChillNum).Base.CWBranchNum,
                             ConstCOPChiller(ChillNum).Base.CWCompNum);
        if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PlantUtilities::SetComponentFlowRate(mdotCond,
                                 CondInletNode,
                                 CondOutletNode,
                                 ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                 ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                 ConstCOPChiller(ChillNum).Base.CDBranchNum,
                                 ConstCOPChiller(ChillNum).Base.CDCompNum);
        }

        if (ConstCOPChiller(ChillNum).Base.CondenserType == EvapCooled) {
            modBasinHeaterPower = 0.0;
        }
    }

    void SizeElectricChiller(int const ChillNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   April 2002
        //       MODIFIED       November 2013 Daeho Kang, add component sizing table entries
        //       RE-ENGINEERED  B. Griffith, April 2011, allow repeated sizing calls, finish when ready to do so

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing Electric Chiller Components for which capacities and flow rates
        // have not been specified in the input.

        // METHODOLOGY EMPLOYED:
        // Obtains evaporator flow rate from the plant sizing array. Calculates nominal capacity from
        // the evaporator flow rate and the chilled water loop design delta T. The condenser flow rate
        // is calculated from the nominal capacity, the COP, and the condenser loop design delta T.
        
        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("SizeElectricChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int PltSizNum(0);        // Plant Sizing index corresponding to CurLoopNum
        int PltSizCondNum(0);    // Plant Sizing index for condenser loop
        bool ErrorsFound(false); // If errors detected in input
        std::string equipName;
        Real64 rho;                               // local fluid density
        Real64 Cp;                                // local fluid specific heat
        Real64 tmpNomCap;                         // local nominal capacity cooling power
        Real64 tmpEvapVolFlowRate;                // local evaporator design volume flow rate
        Real64 tmpCondVolFlowRate;                // local condenser design volume flow rate
        Real64 tmpHeatRecVolFlowRate(0.0);        // local heat recovery design volume flow rate
        Real64 EvapVolFlowRateUser(0.0);          // Hardsized evaporator flow rate for reporting
        Real64 NomCapUser(0.0);                   // Hardsized reference capacity for reporting
        Real64 CondVolFlowRateUser(0.0);          // Hardsized condenser flow rate for reporting
        Real64 DesignHeatRecVolFlowRateUser(0.0); // Hardsized heat recovery flow rate for reporting

        // init local temporary version in case of partial/mixed autosizing
        tmpEvapVolFlowRate = ElectricChiller(ChillNum).Base.EvapVolFlowRate;
        tmpNomCap = ElectricChiller(ChillNum).Base.NomCap;
        tmpCondVolFlowRate = ElectricChiller(ChillNum).Base.CondVolFlowRate;

        if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PltSizCondNum = DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).PlantSizNum;
        }

        PltSizNum = DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).PlantSizNum;

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                       RoutineName);
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidName,
                                           DataGlobals::CWInitConvTemp,
                                           DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                           RoutineName);
                tmpNomCap =
                    Cp * rho * DataSizing::PlantSizData(PltSizNum).DeltaT * DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * ElectricChiller(ChillNum).Base.SizFac;
            } else {
                if (ElectricChiller(ChillNum).Base.NomCapWasAutoSized) tmpNomCap = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (ElectricChiller(ChillNum).Base.NomCapWasAutoSized) {
                    ElectricChiller(ChillNum).Base.NomCap = tmpNomCap;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric", ElectricChiller(ChillNum).Base.Name, "Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(
                            "Chiller:Electric", ElectricChiller(ChillNum).Base.Name, "Initial Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                } else {
                    if (ElectricChiller(ChillNum).Base.NomCap > 0.0 && tmpNomCap > 0.0) {
                        NomCapUser = ElectricChiller(ChillNum).Base.NomCap;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                               ElectricChiller(ChillNum).Base.Name,
                                               "Design Size Nominal Capacity [W]",
                                               tmpNomCap,
                                               "User-Specified Nominal Capacity [W]",
                                               NomCapUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpNomCap - NomCapUser) / NomCapUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerElectric: Potential issue with equipment sizing for " +
                                                ElectricChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Nominal Capacity of " + RoundSigDigits(NomCapUser, 2) + " [W]");
                                    ShowContinueError("differs from Design Size Nominal Capacity of " + RoundSigDigits(tmpNomCap, 2) + " [W]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpNomCap = NomCapUser;
                    }
                }
            }
        } else {
            if (ElectricChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Electric Chiller nominal capacity requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Electric Chiller object=" + ElectricChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!ElectricChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFinalSizesOkayToReport && (ElectricChiller(ChillNum).Base.NomCap > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                   ElectricChiller(ChillNum).Base.Name,
                                   "User-Specified Nominal Capacity [W]",
                                   ElectricChiller(ChillNum).Base.NomCap);
            }
        }

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                tmpEvapVolFlowRate = DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * ElectricChiller(ChillNum).Base.SizFac;
            } else {
                if (ElectricChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (ElectricChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) {
                    ElectricChiller(ChillNum).Base.EvapVolFlowRate = tmpEvapVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                           ElectricChiller(ChillNum).Base.Name,
                                           "Design Size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                           ElectricChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                } else {
                    if (ElectricChiller(ChillNum).Base.EvapVolFlowRate > 0.0 && tmpEvapVolFlowRate > 0.0) {
                        EvapVolFlowRateUser = ElectricChiller(ChillNum).Base.EvapVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                               ElectricChiller(ChillNum).Base.Name,
                                               "Design Size Design Chilled Water Flow Rate [m3/s]",
                                               tmpEvapVolFlowRate,
                                               "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                               EvapVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpEvapVolFlowRate - EvapVolFlowRateUser) / EvapVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerElectric: Potential issue with equipment sizing for " +
                                                ElectricChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Chilled Water Flow Rate of " + RoundSigDigits(EvapVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Chilled Water Flow Rate of " +
                                                      RoundSigDigits(tmpEvapVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpEvapVolFlowRate = EvapVolFlowRateUser;
                    }
                }
            }
        } else {
            if (ElectricChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Electric Chiller evap flow rate requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Electric Chiller object=" + ElectricChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!ElectricChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (ElectricChiller(ChillNum).Base.EvapVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                   ElectricChiller(ChillNum).Base.Name,
                                   "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                   ElectricChiller(ChillNum).Base.EvapVolFlowRate);
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(ElectricChiller(ChillNum).Base.EvapInletNodeNum, tmpEvapVolFlowRate);

        if (PltSizCondNum > 0 && PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow && tmpNomCap > 0.0) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       ElectricChiller(ChillNum).TempDesCondIn,
                                       DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidName,
                                           ElectricChiller(ChillNum).TempDesCondIn,
                                           DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                           RoutineName);
                tmpCondVolFlowRate = tmpNomCap * (1.0 + 1.0 / ElectricChiller(ChillNum).Base.COP) / (DataSizing::PlantSizData(PltSizCondNum).DeltaT * Cp * rho);
            } else {
                if (ElectricChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (ElectricChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) {
                    ElectricChiller(ChillNum).Base.CondVolFlowRate = tmpCondVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                           ElectricChiller(ChillNum).Base.Name,
                                           "Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                           ElectricChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                } else {
                    if (ElectricChiller(ChillNum).Base.CondVolFlowRate > 0.0 && tmpCondVolFlowRate > 0.0) {
                        CondVolFlowRateUser = ElectricChiller(ChillNum).Base.CondVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                               ElectricChiller(ChillNum).Base.Name,
                                               "Design Size Design Condenser Water Flow Rate [m3/s]",
                                               tmpCondVolFlowRate,
                                               "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                               CondVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpCondVolFlowRate - CondVolFlowRateUser) / CondVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerElectric: Potential issue with equipment sizing for " +
                                                ElectricChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Condenser Water Flow Rate of " + RoundSigDigits(CondVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Condenser Water Flow Rate of " +
                                                      RoundSigDigits(tmpCondVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpCondVolFlowRate = CondVolFlowRateUser;
                    }
                }
            }
        } else {
            if (ElectricChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Electric Chiller condenser flow rate requires a condenser");
                ShowContinueError("loop Sizing:Plant object");
                ShowContinueError("Occurs in Electric Chiller object=" + ElectricChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!ElectricChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (ElectricChiller(ChillNum).Base.CondVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                   ElectricChiller(ChillNum).Base.Name,
                                   "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                   ElectricChiller(ChillNum).Base.CondVolFlowRate);
            }
        }

        // save the design condenser water volumetric flow rate for use by the condenser water loop sizing algorithms
        if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PlantUtilities::RegisterPlantCompDesignFlow(ElectricChiller(ChillNum).Base.CondInletNodeNum, tmpCondVolFlowRate);
        }
        if (ErrorsFound) {
            ShowFatalError("Preceding sizing errors cause program termination");
        }

        if (ElectricChiller(ChillNum).HeatRecActive) {
            tmpHeatRecVolFlowRate = ElectricChiller(ChillNum).Base.CondVolFlowRate * ElectricChiller(ChillNum).HeatRecCapacityFraction;
            if (!ElectricChiller(ChillNum).DesignHeatRecVolFlowRateWasAutoSized)
                tmpHeatRecVolFlowRate = ElectricChiller(ChillNum).DesignHeatRecVolFlowRate;
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (ElectricChiller(ChillNum).DesignHeatRecVolFlowRateWasAutoSized) {
                    ElectricChiller(ChillNum).DesignHeatRecVolFlowRate = tmpHeatRecVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                           ElectricChiller(ChillNum).Base.Name,
                                           "Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                           ElectricChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                } else {
                    if (ElectricChiller(ChillNum).DesignHeatRecVolFlowRate > 0.0 && tmpHeatRecVolFlowRate > 0.0) {
                        DesignHeatRecVolFlowRateUser = ElectricChiller(ChillNum).DesignHeatRecVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:Electric",
                                               ElectricChiller(ChillNum).Base.Name,
                                               "Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                               tmpHeatRecVolFlowRate,
                                               "User-Specified Design Heat Recovery Fluid Flow Rate [m3/s]",
                                               DesignHeatRecVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpHeatRecVolFlowRate - DesignHeatRecVolFlowRateUser) / DesignHeatRecVolFlowRateUser) >
                                    DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerElectric: Potential issue with equipment sizing for " +
                                                ElectricChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Heat Recovery Fluid Flow Rate of " +
                                                      RoundSigDigits(DesignHeatRecVolFlowRateUser, 5) + " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Heat Recovery Fluid Flow Rate of " +
                                                      RoundSigDigits(tmpHeatRecVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpHeatRecVolFlowRate = DesignHeatRecVolFlowRateUser;
                    }
                }
            }
            // save the reference heat recovery fluid volumetric flow rate
            PlantUtilities::RegisterPlantCompDesignFlow(ElectricChiller(ChillNum).HeatRecInletNodeNum, tmpHeatRecVolFlowRate);
        }

        if (DataPlant::PlantFinalSizesOkayToReport) {
            // create predefined report
            equipName = ElectricChiller(ChillNum).Base.Name;
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, "Chiller:Electric");
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomEff, equipName, ElectricChiller(ChillNum).Base.COP);
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, ElectricChiller(ChillNum).Base.NomCap);
        }
    }

    void SizeEngineDrivenChiller(int const ChillNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   June 2002
        //       MODIFIED       November 2013 Daeho Kang, add component sizing table entries
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing Engine Driven Chiller Components for which capacities and flow rates
        // have not been specified in the input.

        // METHODOLOGY EMPLOYED:
        // Obtains evaporator flow rate from the plant sizing array. Calculates nominal capacity from
        // the evaporator flow rate and the chilled water loop design delta T. The condenser flow rate
        // is calculated from the nominal capacity, the COP, and the condenser loop design delta T.

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("SizeEngineDrivenChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int PltSizNum;     // Plant Sizing index corresponding to CurLoopNum
        int PltSizCondNum; // Plant Sizing index for condenser loop
        bool ErrorsFound;  // If errors detected in input
        std::string equipName;
        Real64 rho;                 // local fluid density
        Real64 Cp;                  // local fluid specific heat
        Real64 tmpNomCap;           // local nominal capacity cooling power
        Real64 tmpEvapVolFlowRate;  // local evaporator design volume flow rate
        Real64 tmpCondVolFlowRate;  // local condenser design volume flow rate
        Real64 EvapVolFlowRateUser; // Hardsized evaporator flow rate for reporting
        Real64 NomCapUser;          // Hardsized reference capacity for reporting
        Real64 CondVolFlowRateUser; // Hardsized condenser flow rate for reporting

        PltSizNum = 0;
        PltSizCondNum = 0;
        ErrorsFound = false;
        tmpNomCap = EngineDrivenChiller(ChillNum).Base.NomCap;
        tmpEvapVolFlowRate = EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate;
        tmpCondVolFlowRate = EngineDrivenChiller(ChillNum).Base.CondVolFlowRate;
        EvapVolFlowRateUser = 0.0;
        NomCapUser = 0.0;
        CondVolFlowRateUser = 0.0;

        if (EngineDrivenChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PltSizCondNum = DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).PlantSizNum;
        }

        PltSizNum = DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).PlantSizNum;

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                       RoutineName);
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).FluidName,
                                           DataGlobals::CWInitConvTemp,
                                           DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                           RoutineName);
                tmpNomCap =
                    Cp * rho * DataSizing::PlantSizData(PltSizNum).DeltaT * DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * EngineDrivenChiller(ChillNum).Base.SizFac;
            } else {
                if (EngineDrivenChiller(ChillNum).Base.NomCapWasAutoSized) tmpNomCap = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (EngineDrivenChiller(ChillNum).Base.NomCapWasAutoSized) {
                    EngineDrivenChiller(ChillNum).Base.NomCap = tmpNomCap;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(
                            "Chiller:EngineDriven", EngineDrivenChiller(ChillNum).Base.Name, "Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(
                            "Chiller:EngineDriven", EngineDrivenChiller(ChillNum).Base.Name, "Initial Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                } else {
                    if (EngineDrivenChiller(ChillNum).Base.NomCap > 0.0 && tmpNomCap > 0.0) {
                        NomCapUser = EngineDrivenChiller(ChillNum).Base.NomCap;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                               EngineDrivenChiller(ChillNum).Base.Name,
                                               "Design Size Nominal Capacity [W]",
                                               tmpNomCap,
                                               "User-Specified Nominal Capacity [W]",
                                               NomCapUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpNomCap - NomCapUser) / NomCapUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerEngineDriven: Potential issue with equipment sizing for " +
                                                EngineDrivenChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Nominal Capacity of " + RoundSigDigits(NomCapUser, 2) + " [W]");
                                    ShowContinueError("differs from Design Size Nominal Capacity of " + RoundSigDigits(tmpNomCap, 2) + " [W]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpNomCap = NomCapUser;
                    }
                }
            }
        } else {
            if (EngineDrivenChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Engine Driven Chiller nominal capacity requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Engine Driven Chiller object=" + EngineDrivenChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!EngineDrivenChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (EngineDrivenChiller(ChillNum).Base.NomCap > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                   EngineDrivenChiller(ChillNum).Base.Name,
                                   "User-Specified Nominal Capacity [W]",
                                   EngineDrivenChiller(ChillNum).Base.NomCap);
            }
        }

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                tmpEvapVolFlowRate = DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * EngineDrivenChiller(ChillNum).Base.SizFac;
            } else {
                if (EngineDrivenChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (EngineDrivenChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) {
                    EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate = tmpEvapVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                           EngineDrivenChiller(ChillNum).Base.Name,
                                           "Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                           EngineDrivenChiller(ChillNum).Base.Name,
                                           "Initial Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                } else {
                    if (EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate > 0.0 && tmpEvapVolFlowRate > 0.0) {
                        EvapVolFlowRateUser = EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                               EngineDrivenChiller(ChillNum).Base.Name,
                                               "Design Size Design Chilled Water Flow Rate [m3/s]",
                                               tmpEvapVolFlowRate,
                                               "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                               EvapVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpEvapVolFlowRate - EvapVolFlowRateUser) / EvapVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerEngineDriven: Potential issue with equipment sizing for " +
                                                EngineDrivenChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Chilled Water Flow Rate of " + RoundSigDigits(EvapVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Chilled Water Flow Rate of " +
                                                      RoundSigDigits(tmpEvapVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpEvapVolFlowRate = EvapVolFlowRateUser;
                    }
                }
            }
        } else {
            if (EngineDrivenChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Engine Driven Chiller evap flow rate requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Engine Driven Chiller object=" + EngineDrivenChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!EngineDrivenChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                   EngineDrivenChiller(ChillNum).Base.Name,
                                   "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                   EngineDrivenChiller(ChillNum).Base.EvapVolFlowRate);
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(EngineDrivenChiller(ChillNum).Base.EvapInletNodeNum, tmpEvapVolFlowRate);

        if (PltSizCondNum > 0 && PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow && tmpNomCap > 0.0) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       EngineDrivenChiller(ChillNum).TempDesCondIn,
                                       DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);

                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).FluidName,
                                           EngineDrivenChiller(ChillNum).TempDesCondIn,
                                           DataPlant::PlantLoop(EngineDrivenChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                           RoutineName);
                tmpCondVolFlowRate =
                    tmpNomCap * (1.0 + 1.0 / EngineDrivenChiller(ChillNum).Base.COP) / (DataSizing::PlantSizData(PltSizCondNum).DeltaT * Cp * rho);
            } else {
                if (EngineDrivenChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (EngineDrivenChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) {
                    EngineDrivenChiller(ChillNum).Base.CondVolFlowRate = tmpCondVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                           EngineDrivenChiller(ChillNum).Base.Name,
                                           "Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                           EngineDrivenChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                } else {
                    if (EngineDrivenChiller(ChillNum).Base.CondVolFlowRate > 0.0 && tmpCondVolFlowRate > 0.0) {
                        CondVolFlowRateUser = EngineDrivenChiller(ChillNum).Base.CondVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                               EngineDrivenChiller(ChillNum).Base.Name,
                                               "Design Size Design Condenser Water Flow Rate [m3/s]",
                                               tmpCondVolFlowRate,
                                               "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                               CondVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpCondVolFlowRate - CondVolFlowRateUser) / CondVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerEngineDriven: Potential issue with equipment sizing for " +
                                                EngineDrivenChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Condenser Water Flow Rate of " + RoundSigDigits(CondVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Condenser Water Flow Rate of " +
                                                      RoundSigDigits(tmpCondVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpCondVolFlowRate = CondVolFlowRateUser;
                    }
                }
            }
        } else {
            if (EngineDrivenChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of EngineDriven Chiller condenser flow rate requires a condenser");
                ShowContinueError("loop Sizing:Plant object");
                ShowContinueError("Occurs in EngineDriven Chiller object=" + EngineDrivenChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!EngineDrivenChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (EngineDrivenChiller(ChillNum).Base.CondVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                   EngineDrivenChiller(ChillNum).Base.Name,
                                   "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                   EngineDrivenChiller(ChillNum).Base.CondVolFlowRate);
            }
        }

        // save the design condenser water volumetric flow rate for use by the condenser water loop sizing algorithms
        if (EngineDrivenChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PlantUtilities::RegisterPlantCompDesignFlow(EngineDrivenChiller(ChillNum).Base.CondInletNodeNum, tmpCondVolFlowRate);
        }

        // autosize support for heat recovery flow rate.
        if (EngineDrivenChiller(ChillNum).HeatRecActive) {
            Real64 tmpHeatRecVolFlowRate = tmpCondVolFlowRate * EngineDrivenChiller(ChillNum).HeatRecCapacityFraction;
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRateWasAutoSized) {
                    EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRate = tmpHeatRecVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                           EngineDrivenChiller(ChillNum).Base.Name,
                                           "Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                           EngineDrivenChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                } else {
                    if (EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRate > 0.0 && tmpHeatRecVolFlowRate > 0.0) {
                        Real64 DesignHeatRecVolFlowRateUser = EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            if (DataGlobals::DoPlantSizing) {
                                ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                                   EngineDrivenChiller(ChillNum).Base.Name,
                                                   "Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                                   tmpHeatRecVolFlowRate,
                                                   "User-Specified Design Heat Recovery Fluid Flow Rate [m3/s]",
                                                   DesignHeatRecVolFlowRateUser);
                            } else {
                                ReportSizingManager::ReportSizingOutput("Chiller:EngineDriven",
                                                   EngineDrivenChiller(ChillNum).Base.Name,
                                                   "User-Specified Design Heat Recovery Fluid Flow Rate [m3/s]",
                                                   DesignHeatRecVolFlowRateUser);
                            }
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpHeatRecVolFlowRate - DesignHeatRecVolFlowRateUser) / DesignHeatRecVolFlowRateUser) >
                                    DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeEngineDrivenChiller: Potential issue with equipment sizing for " +
                                                EngineDrivenChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Heat Recovery Fluid Flow Rate of " +
                                                      RoundSigDigits(DesignHeatRecVolFlowRateUser, 5) + " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Heat Recovery Fluid Flow Rate of " +
                                                      RoundSigDigits(tmpHeatRecVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpHeatRecVolFlowRate = DesignHeatRecVolFlowRateUser;
                    }
                }
            }
            if (!EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRateWasAutoSized)
                tmpHeatRecVolFlowRate = EngineDrivenChiller(ChillNum).DesignHeatRecVolFlowRate;
            // save the reference heat recovery fluid volumetric flow rate
            PlantUtilities::RegisterPlantCompDesignFlow(EngineDrivenChiller(ChillNum).HeatRecInletNodeNum, tmpHeatRecVolFlowRate);
        }

        if (DataPlant::PlantFinalSizesOkayToReport) {
            // create predefined report
            equipName = EngineDrivenChiller(ChillNum).Base.Name;
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, "Chiller:EngineDriven");
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomEff, equipName, EngineDrivenChiller(ChillNum).Base.COP);
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, EngineDrivenChiller(ChillNum).Base.NomCap);
        }

        if (ErrorsFound) {
            ShowFatalError("Preceding sizing errors cause program termination");
        }
    }

    void SizeGTChiller(int const ChillNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   June 2002
        //       MODIFIED       November 2013 Daeho Kang, add component sizing table entries
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing Gas Turbine Chiller Components for which capacities and flow rates
        // have not been specified in the input.

        // METHODOLOGY EMPLOYED:
        // Obtains evaporator flow rate from the plant sizing array. Calculates nominal capacity from
        // the evaporator flow rate and the chilled water loop design delta T. The condenser flow rate
        // is calculated from the nominal capacity, the COP, and the condenser loop design delta T.

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("SizeGTChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int PltSizNum;     // Plant Sizing index corresponding to CurLoopNum
        int PltSizCondNum; // Plant Sizing index for condenser loop
        bool ErrorsFound;  // If errors detected in input
        std::string equipName;
        Real64 rho;                  // local fluid density
        Real64 Cp;                   // local fluid specific heat
        Real64 tmpNomCap;            // local nominal capacity cooling power
        Real64 tmpEvapVolFlowRate;   // local evaporator design volume flow rate
        Real64 tmpCondVolFlowRate;   // local condenser design volume flow rate
        Real64 EvapVolFlowRateUser;  // Hardsized evaporator flow rate for reporting
        Real64 NomCapUser;           // Hardsized reference capacity for reporting
        Real64 CondVolFlowRateUser;  // Hardsized condenser flow rate for reporting
        Real64 GTEngineCapacityDes;  // Autosized GT engine capacity for reporting
        Real64 GTEngineCapacityUser; // Hardsized GT engine capacity for reporting

        PltSizNum = 0;
        PltSizCondNum = 0;
        ErrorsFound = false;
        tmpNomCap = GTChiller(ChillNum).Base.NomCap;
        tmpEvapVolFlowRate = GTChiller(ChillNum).Base.EvapVolFlowRate;
        tmpCondVolFlowRate = GTChiller(ChillNum).Base.CondVolFlowRate;
        EvapVolFlowRateUser = 0.0;
        NomCapUser = 0.0;
        CondVolFlowRateUser = 0.0;
        GTEngineCapacityDes = 0.0;
        GTEngineCapacityUser = 0.0;

        if (GTChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PltSizCondNum = DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).PlantSizNum;
        }

        PltSizNum = DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).PlantSizNum;

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                       RoutineName);
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).FluidName,
                                           DataGlobals::CWInitConvTemp,
                                           DataPlant::PlantLoop(GTChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                           RoutineName);
                tmpNomCap = Cp * rho * DataSizing::PlantSizData(PltSizNum).DeltaT * DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * GTChiller(ChillNum).Base.SizFac;
            } else {
                if (GTChiller(ChillNum).Base.NomCapWasAutoSized) tmpNomCap = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GTChiller(ChillNum).Base.NomCapWasAutoSized) {
                    GTChiller(ChillNum).Base.NomCap = tmpNomCap;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine", GTChiller(ChillNum).Base.Name, "Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(
                            "Chiller:CombustionTurbine", GTChiller(ChillNum).Base.Name, "Initial Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                } else {
                    if (GTChiller(ChillNum).Base.NomCap > 0.0 && tmpNomCap > 0.0) {
                        NomCapUser = GTChiller(ChillNum).Base.NomCap;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                               GTChiller(ChillNum).Base.Name,
                                               "Design Size Nominal Capacity [W]",
                                               tmpNomCap,
                                               "User-Specified Nominal Capacity [W]",
                                               NomCapUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpNomCap - NomCapUser) / NomCapUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeGTChiller: Potential issue with equipment sizing for " + GTChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Nominal Capacity of " + RoundSigDigits(NomCapUser, 2) + " [W]");
                                    ShowContinueError("differs from Design Size Nominal Capacity of " + RoundSigDigits(tmpNomCap, 2) + " [W]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpNomCap = NomCapUser;
                    }
                }
            }
        } else {
            if (GTChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Gas Turbine Chiller nominal capacity requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Gas Turbine Chiller object=" + GTChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!GTChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFinalSizesOkayToReport && (GTChiller(ChillNum).Base.NomCap > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                   GTChiller(ChillNum).Base.Name,
                                   "User-Specified Design Size Nominal Capacity [W]",
                                   GTChiller(ChillNum).Base.NomCap);
            }
        }

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                tmpEvapVolFlowRate = DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * GTChiller(ChillNum).Base.SizFac;
            } else {
                if (GTChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GTChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) {
                    GTChiller(ChillNum).Base.EvapVolFlowRate = tmpEvapVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Design size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Initial Design size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                } else {
                    if (GTChiller(ChillNum).Base.EvapVolFlowRate > 0.0 && tmpEvapVolFlowRate > 0.0) {
                        EvapVolFlowRateUser = GTChiller(ChillNum).Base.EvapVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                               GTChiller(ChillNum).Base.Name,
                                               "Design size Design Chilled Water Flow Rate [m3/s]",
                                               tmpEvapVolFlowRate,
                                               "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                               EvapVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpEvapVolFlowRate - EvapVolFlowRateUser) / EvapVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeGTChiller: Potential issue with equipment sizing for " + GTChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Chilled Water Flow Rate of " + RoundSigDigits(EvapVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Chilled Water Flow Rate of " +
                                                      RoundSigDigits(tmpEvapVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpEvapVolFlowRate = EvapVolFlowRateUser;
                    }
                }
            }
        } else {
            if (GTChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Gas Turbine Chiller evap flow rate requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Gas Turbine Chiller object=" + GTChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!GTChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (GTChiller(ChillNum).Base.EvapVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                   GTChiller(ChillNum).Base.Name,
                                   "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                   GTChiller(ChillNum).Base.EvapVolFlowRate);
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(GTChiller(ChillNum).Base.EvapInletNodeNum, tmpEvapVolFlowRate);

        if (PltSizCondNum > 0 && PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow && tmpNomCap > 0.0) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).FluidName,
                                       GTChiller(ChillNum).TempDesCondIn,
                                       DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                       RoutineName);
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).FluidName,
                                           GTChiller(ChillNum).TempDesCondIn,
                                           DataPlant::PlantLoop(GTChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                           RoutineName);
                tmpCondVolFlowRate = tmpNomCap * (1.0 + 1.0 / GTChiller(ChillNum).Base.COP) / (DataSizing::PlantSizData(PltSizCondNum).DeltaT * Cp * rho);
            } else {
                if (GTChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GTChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) {
                    GTChiller(ChillNum).Base.CondVolFlowRate = tmpCondVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Condenser Water Flow Rate [m3/s]",
                                           tmpCondVolFlowRate);
                    }
                } else {
                    if (GTChiller(ChillNum).Base.CondVolFlowRate > 0.0 && tmpCondVolFlowRate > 0.0) {
                        CondVolFlowRateUser = GTChiller(ChillNum).Base.CondVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                               GTChiller(ChillNum).Base.Name,
                                               "Design Size Design Condenser Water Flow Rate [m3/s]",
                                               tmpCondVolFlowRate,
                                               "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                               CondVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpCondVolFlowRate - CondVolFlowRateUser) / CondVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeGTChiller: Potential issue with equipment sizing for " + GTChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Condenser Water Flow Rate of " + RoundSigDigits(CondVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Condenser Water Flow Rate of " +
                                                      RoundSigDigits(tmpCondVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpCondVolFlowRate = CondVolFlowRateUser;
                    }
                }
            }
        } else {
            if (GTChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Gas Turbine Chiller condenser flow rate requires a condenser");
                ShowContinueError("loop Sizing:Plant object");
                ShowContinueError("Occurs in Gas Turbine Chiller object=" + GTChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!GTChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (GTChiller(ChillNum).Base.CondVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                   GTChiller(ChillNum).Base.Name,
                                   "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                   GTChiller(ChillNum).Base.CondVolFlowRate);
            }
        }
        // save the design condenser water volumetric flow rate for use by the condenser water loop sizing algorithms
        if (GTChiller(ChillNum).Base.CondenserType == WaterCooled)
            PlantUtilities::RegisterPlantCompDesignFlow(GTChiller(ChillNum).Base.CondInletNodeNum, tmpCondVolFlowRate);

        GTEngineCapacityDes = GTChiller(ChillNum).Base.NomCap / (GTChiller(ChillNum).engineCapacityScalar * GTChiller(ChillNum).Base.COP);
        if (DataPlant::PlantFirstSizesOkayToFinalize) {
            if (GTChiller(ChillNum).GTEngineCapacityWasAutoSized) {
                GTChiller(ChillNum).GTEngineCapacity = GTEngineCapacityDes;
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                       GTChiller(ChillNum).Base.Name,
                                       "Design Size Gas Turbine Engine Capacity [W]",
                                       GTEngineCapacityDes);
                }
                if (DataPlant::PlantFirstSizesOkayToReport) {
                    ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                       GTChiller(ChillNum).Base.Name,
                                       "Initial Design Size Gas Turbine Engine Capacity [W]",
                                       GTEngineCapacityDes);
                }
            } else {
                if (GTChiller(ChillNum).GTEngineCapacity > 0.0 && GTEngineCapacityDes > 0.0) {
                    GTEngineCapacityUser = GTChiller(ChillNum).GTEngineCapacity;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Design Size Gas Turbine Engine Capacity [W]",
                                           GTEngineCapacityDes,
                                           "User-Specified Gas Turbine Engine Capacity [W]",
                                           GTEngineCapacityUser);
                    }
                    if (DisplayExtraWarnings) {
                        if ((std::abs(GTEngineCapacityDes - GTEngineCapacityUser) / GTEngineCapacityUser) > DataSizing::AutoVsHardSizingThreshold) {
                            ShowMessage("SizeGTChiller: Potential issue with equipment sizing for " + GTChiller(ChillNum).Base.Name);
                            ShowContinueError("User-Specified Gas Turbine Engine Capacity of " + RoundSigDigits(GTEngineCapacityUser, 2) + " [W]");
                            ShowContinueError("differs from Design Size Gas Turbine Engine Capacity of " + RoundSigDigits(GTEngineCapacityDes, 2) +
                                              " [W]");
                            ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                            ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                        }
                    }
                }
            }
        }

        // autosize support for heat recovery flow rate.
        if (GTChiller(ChillNum).HeatRecActive) {
            Real64 tmpHeatRecVolFlowRate = GTChiller(ChillNum).Base.CondVolFlowRate * GTChiller(ChillNum).HeatRecCapacityFraction;
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (GTChiller(ChillNum).DesignHeatRecVolFlowRateWasAutoSized) {
                    GTChiller(ChillNum).DesignHeatRecVolFlowRate = tmpHeatRecVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                           GTChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                           tmpHeatRecVolFlowRate);
                    }
                } else {
                    if (GTChiller(ChillNum).DesignHeatRecVolFlowRate > 0.0 && tmpHeatRecVolFlowRate > 0.0) {
                        Real64 DesignHeatRecVolFlowRateUser = GTChiller(ChillNum).DesignHeatRecVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            if (DataGlobals::DoPlantSizing) {
                                ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                                   GTChiller(ChillNum).Base.Name,
                                                   "Design Size Design Heat Recovery Fluid Flow Rate [m3/s]",
                                                   tmpHeatRecVolFlowRate,
                                                   "User-Specified Design Heat Recovery Fluid Flow Rate [m3/s]",
                                                   DesignHeatRecVolFlowRateUser);
                            } else {
                                ReportSizingManager::ReportSizingOutput("Chiller:CombustionTurbine",
                                                   GTChiller(ChillNum).Base.Name,
                                                   "User-Specified Design Heat Recovery Fluid Flow Rate [m3/s]",
                                                   DesignHeatRecVolFlowRateUser);
                            }
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpHeatRecVolFlowRate - DesignHeatRecVolFlowRateUser) / DesignHeatRecVolFlowRateUser) >
                                    DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeEngineDrivenChiller: Potential issue with equipment sizing for " +
                                                EngineDrivenChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Heat Recovery Fluid Flow Rate of " +
                                                      RoundSigDigits(DesignHeatRecVolFlowRateUser, 5) + " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Heat Recovery Fluid Flow Rate of " +
                                                      RoundSigDigits(tmpHeatRecVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpHeatRecVolFlowRate = DesignHeatRecVolFlowRateUser;
                    }
                }
            }
            if (!GTChiller(ChillNum).DesignHeatRecVolFlowRateWasAutoSized) tmpHeatRecVolFlowRate = GTChiller(ChillNum).DesignHeatRecVolFlowRate;
            // save the reference heat recovery fluid volumetric flow rate
            PlantUtilities::RegisterPlantCompDesignFlow(GTChiller(ChillNum).HeatRecInletNodeNum, tmpHeatRecVolFlowRate);
        }

        if (DataPlant::PlantFinalSizesOkayToReport) {
            // create predefined report
            equipName = GTChiller(ChillNum).Base.Name;
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, "Chiller:CombustionTurbine");
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomEff, equipName, GTChiller(ChillNum).Base.COP);
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, GTChiller(ChillNum).Base.NomCap);
        }

        if (ErrorsFound) {
            ShowFatalError("Preceding sizing errors cause program termination");
        }
    }

    void SizeConstCOPChiller(int const ChillNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   March 2008
        //       MODIFIED       November 2013 Daeho Kang, add component sizing table entries
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing Constabt COP Chiller Components for which capacities and flow rates
        // have not been specified in the input.

        // METHODOLOGY EMPLOYED:
        // Obtains evaporator flow rate from the plant sizing array. Calculates nominal capacity from
        // the evaporator flow rate and the chilled water loop design delta T. The condenser flow rate
        // is calculated from the nominal capacity, the COP, and the condenser loop design delta T.

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("SizeConstCOPChiller");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int PltSizNum;     // Plant Sizing index corresponding to CurLoopNum
        int PltSizCondNum; // Plant Sizing index for condenser loop
        bool ErrorsFound;  // If errors detected in input
        std::string equipName;
        Real64 rho;                 // local fluid density
        Real64 Cp;                  // local fluid specific heat
        Real64 tmpNomCap;           // local nominal capacity cooling power
        Real64 tmpEvapVolFlowRate;  // local evaporator design volume flow rate
        Real64 tmpCondVolFlowRate;  // local condenser design volume flow rate
        Real64 EvapVolFlowRateUser; // Hardsized evaporator flow for reporting
        Real64 NomCapUser;          // Hardsized reference capacity for reporting
        Real64 CondVolFlowRateUser; // Hardsized condenser flow for reporting

        PltSizNum = 0;
        PltSizCondNum = 0;
        ErrorsFound = false;
        tmpNomCap = ConstCOPChiller(ChillNum).Base.NomCap;
        tmpEvapVolFlowRate = ConstCOPChiller(ChillNum).Base.EvapVolFlowRate;
        tmpCondVolFlowRate = ConstCOPChiller(ChillNum).Base.CondVolFlowRate;

        EvapVolFlowRateUser = 0.0;
        NomCapUser = 0.0;
        CondVolFlowRateUser = 0.0;

        if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
            PltSizCondNum = DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).PlantSizNum;
        }

        PltSizNum = DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).PlantSizNum;

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidName,
                                       DataGlobals::CWInitConvTemp,
                                       DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                       RoutineName);
                Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidName,
                                           DataGlobals::CWInitConvTemp,
                                           DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                           RoutineName);
                tmpNomCap =
                    Cp * rho * DataSizing::PlantSizData(PltSizNum).DeltaT * DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * ConstCOPChiller(ChillNum).Base.SizFac;
            } else {
                if (ConstCOPChiller(ChillNum).Base.NomCapWasAutoSized) tmpNomCap = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (ConstCOPChiller(ChillNum).Base.NomCapWasAutoSized) {
                    ConstCOPChiller(ChillNum).Base.NomCap = tmpNomCap;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP", ConstCOPChiller(ChillNum).Base.Name, "Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(
                            "Chiller:ConstantCOP", ConstCOPChiller(ChillNum).Base.Name, "Initial Design Size Nominal Capacity [W]", tmpNomCap);
                    }
                } else { // Hard-size with sizing data
                    if (ConstCOPChiller(ChillNum).Base.NomCap > 0.0 && tmpNomCap > 0.0) {
                        NomCapUser = ConstCOPChiller(ChillNum).Base.NomCap;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                               ConstCOPChiller(ChillNum).Base.Name,
                                               "Design Size Nominal Capacity [W]",
                                               tmpNomCap,
                                               "User-Specified Nominal Capacity [W]",
                                               NomCapUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpNomCap - NomCapUser) / NomCapUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerConstantCOP: Potential issue with equipment sizing for " +
                                                ConstCOPChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Nominal Capacity of " + RoundSigDigits(NomCapUser, 2) + " [W]");
                                    ShowContinueError("differs from Design Size Nominal Capacity of " + RoundSigDigits(tmpNomCap, 2) + " [W]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpNomCap = NomCapUser;
                    }
                }
            }
        } else {
            if (ConstCOPChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Constant COP Chiller nominal capacity requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Chiller:ConstantCOP object=" + ConstCOPChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!ConstCOPChiller(ChillNum).Base.NomCapWasAutoSized && DataPlant::PlantFinalSizesOkayToReport && (ConstCOPChiller(ChillNum).Base.NomCap > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                   ConstCOPChiller(ChillNum).Base.Name,
                                   "User-Specified Nominal Capacity [W]",
                                   ConstCOPChiller(ChillNum).Base.NomCap);
            }
        }

        if (PltSizNum > 0) {
            if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow) {
                tmpEvapVolFlowRate = DataSizing::PlantSizData(PltSizNum).DesVolFlowRate * ConstCOPChiller(ChillNum).Base.SizFac;
            } else {
                if (ConstCOPChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) tmpEvapVolFlowRate = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (ConstCOPChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized) {
                    ConstCOPChiller(ChillNum).Base.EvapVolFlowRate = tmpEvapVolFlowRate;
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                           ConstCOPChiller(ChillNum).Base.Name,
                                           "Design Size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                           ConstCOPChiller(ChillNum).Base.Name,
                                           "Initial Design Size Design Chilled Water Flow Rate [m3/s]",
                                           tmpEvapVolFlowRate);
                    }
                } else {
                    if (ConstCOPChiller(ChillNum).Base.EvapVolFlowRate > 0.0 && tmpEvapVolFlowRate > 0.0) {
                        EvapVolFlowRateUser = ConstCOPChiller(ChillNum).Base.EvapVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                               ConstCOPChiller(ChillNum).Base.Name,
                                               "Design Size Design Chilled Water Flow Rate [m3/s]",
                                               tmpEvapVolFlowRate,
                                               "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                               EvapVolFlowRateUser);
                            if (DisplayExtraWarnings) {
                                if ((std::abs(tmpEvapVolFlowRate - EvapVolFlowRateUser) / EvapVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                    ShowMessage("SizeChillerConstantCOP: Potential issue with equipment sizing for " +
                                                ConstCOPChiller(ChillNum).Base.Name);
                                    ShowContinueError("User-Specified Design Chilled Water Flow Rate of " + RoundSigDigits(EvapVolFlowRateUser, 5) +
                                                      " [m3/s]");
                                    ShowContinueError("differs from Design Size Design Chilled Water Flow Rate of " +
                                                      RoundSigDigits(tmpEvapVolFlowRate, 5) + " [m3/s]");
                                    ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                        tmpEvapVolFlowRate = EvapVolFlowRateUser;
                    }
                }
            }
        } else {
            if (ConstCOPChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                ShowSevereError("Autosizing of Constant COP Chiller evap flow rate requires a loop Sizing:Plant object");
                ShowContinueError("Occurs in Chiller:ConstantCOP object=" + ConstCOPChiller(ChillNum).Base.Name);
                ErrorsFound = true;
            }
            if (!ConstCOPChiller(ChillNum).Base.EvapVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                (ConstCOPChiller(ChillNum).Base.EvapVolFlowRate > 0.0)) {
                ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                   ConstCOPChiller(ChillNum).Base.Name,
                                   "User-Specified Design Chilled Water Flow Rate [m3/s]",
                                   ConstCOPChiller(ChillNum).Base.EvapVolFlowRate);
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(ConstCOPChiller(ChillNum).Base.EvapInletNodeNum, tmpEvapVolFlowRate);

        if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
            if (PltSizCondNum > 0 && PltSizNum > 0) {
                if (DataSizing::PlantSizData(PltSizNum).DesVolFlowRate >= SmallWaterVolFlow && tmpNomCap > 0.0) {
                    rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidName,
                                           29.44,
                                           DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                           RoutineName);
                    Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidName,
                                               29.44,
                                               DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                               RoutineName);
                    tmpCondVolFlowRate =
                        tmpNomCap * (1.0 + 1.0 / ConstCOPChiller(ChillNum).Base.COP) / (DataSizing::PlantSizData(PltSizCondNum).DeltaT * Cp * rho);
                } else {
                    if (ConstCOPChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) tmpCondVolFlowRate = 0.0;
                }
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    if (ConstCOPChiller(ChillNum).Base.CondVolFlowRateWasAutoSized) {
                        ConstCOPChiller(ChillNum).Base.CondVolFlowRate = tmpCondVolFlowRate;
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                               ConstCOPChiller(ChillNum).Base.Name,
                                               "Design Size Design Condenser Water Flow Rate [m3/s]",
                                               tmpCondVolFlowRate);
                        }
                        if (DataPlant::PlantFirstSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                               ConstCOPChiller(ChillNum).Base.Name,
                                               "Initial Design Size Design Condenser Water Flow Rate [m3/s]",
                                               tmpCondVolFlowRate);
                        }
                    } else {
                        if (ConstCOPChiller(ChillNum).Base.CondVolFlowRate > 0.0 && tmpCondVolFlowRate > 0.0) {
                            CondVolFlowRateUser = ConstCOPChiller(ChillNum).Base.CondVolFlowRate;
                            if (DataPlant::PlantFinalSizesOkayToReport) {
                                ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                                   ConstCOPChiller(ChillNum).Base.Name,
                                                   "Design Size Design Condenser Water Flow Rate [m3/s]",
                                                   tmpCondVolFlowRate,
                                                   "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                                   CondVolFlowRateUser);
                                if (DisplayExtraWarnings) {
                                    if ((std::abs(tmpCondVolFlowRate - CondVolFlowRateUser) / CondVolFlowRateUser) > DataSizing::AutoVsHardSizingThreshold) {
                                        ShowMessage("SizeChillerConstantCOP: Potential issue with equipment sizing for " +
                                                    ConstCOPChiller(ChillNum).Base.Name);
                                        ShowContinueError("User-Specified Design Condenser Water Flow Rate of " +
                                                          RoundSigDigits(CondVolFlowRateUser, 5) + " [m3/s]");
                                        ShowContinueError("differs from Design Size Design Condenser Water Flow Rate of " +
                                                          RoundSigDigits(tmpCondVolFlowRate, 5) + " [m3/s]");
                                        ShowContinueError("This may, or may not, indicate mismatched component sizes.");
                                        ShowContinueError("Verify that the value entered is intended and is consistent with other components.");
                                    }
                                }
                            }
                            tmpCondVolFlowRate = CondVolFlowRateUser;
                        }
                    }
                }
            } else {
                if (ConstCOPChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("Autosizing of Constant COP Chiller condenser flow rate requires a condenser");
                    ShowContinueError("loop Sizing:Plant object");
                    ShowContinueError("Occurs in Chiller:ConstantCOP object=" + ConstCOPChiller(ChillNum).Base.Name);
                    ErrorsFound = true;
                }
                if (!ConstCOPChiller(ChillNum).Base.CondVolFlowRateWasAutoSized && DataPlant::PlantFinalSizesOkayToReport &&
                    (ConstCOPChiller(ChillNum).Base.CondVolFlowRate > 0.0)) {
                    ReportSizingManager::ReportSizingOutput("Chiller:ConstantCOP",
                                       ConstCOPChiller(ChillNum).Base.Name,
                                       "User-Specified Design Condenser Water Flow Rate [m3/s]",
                                       ConstCOPChiller(ChillNum).Base.CondVolFlowRate);
                }
            }
        }

        // save the design condenser water volumetric flow rate for use by the condenser water loop sizing algorithms
        if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled)
            PlantUtilities::RegisterPlantCompDesignFlow(ConstCOPChiller(ChillNum).Base.CondInletNodeNum, tmpCondVolFlowRate);

        if (ErrorsFound) {
            ShowFatalError("Preceding sizing errors cause program termination");
        }

        // create predefined report
        if (DataPlant::PlantFinalSizesOkayToReport) {
            equipName = ConstCOPChiller(ChillNum).Base.Name;
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, "Chiller:ConstantCOP");
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomEff, equipName, ConstCOPChiller(ChillNum).Base.COP);
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, ConstCOPChiller(ChillNum).Base.NomCap);
        }
    }

    void CalcElectricChillerModel(int &ChillNum,           // chiller number
                                  Real64 &MyLoad,          // operating load
                                  int const EquipFlowCtrl, // Flow control mode for the equipment
                                  bool const RunFlag       // TRUE when chiller operating
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Dan Fisher / Brandon Anderson
        //       DATE WRITTEN   Sept. 2000
        //       MODIFIED       Feb. 2010, Chandan Sharma, FSEC. Added basin heater
        //                      Jun. 2016, Rongpeng Zhang, LBNL. Applied the chiller supply water temperature sensor fault model
        //                      Nov. 2016, Rongpeng Zhang, LBNL. Added Fouling Chiller fault
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // simulate a vapor compression chiller using the Electric model

        // METHODOLOGY EMPLOYED:
        // curve fit of performance data:

        // REFERENCES:
        // 1. BLAST Users Manual
        // 2. CHILLER User Manual

        // Locals
        Real64 _EvapInletTemp; // C - evaporator inlet temperature, water side
        Real64 _CondInletTemp; // C - condenser inlet temperature, water side

        static ObjexxFCL::gio::Fmt OutputFormat("(F6.2)");
        static std::string const RoutineName("CalcElectricChillerModel");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 MinPartLoadRat;           // min allowed operating frac full load
        Real64 MaxPartLoadRat;           // max allowed operating frac full load
        Real64 TempCondInDesign;         // C - (Electric ADJTC(1)The design secondary loop fluid
        Real64 TempRiseRat;              // intermediate result:  temperature rise ratio
        Real64 TempEvapOut;              // C - evaporator outlet temperature, water side
        Real64 TempEvapOutSetPoint(0.0); // C - evaporator outlet temperature setpoint
        Real64 TempEvapOutDesign;        // design evaporator outlet temperature, water side
        Real64 ChillerNomCap;            // chiller nominal capacity
        Real64 AvailChillerCap;          // chiller available capacity
        Real64 RatedCOP;                 // rated coefficient of performance, from user input
        Real64 FracFullLoadPower;        // fraction of full load power
        Real64 EvapDeltaTemp(0.0);       // C - evaporator temperature difference, water side
        Real64 DeltaTemp;                // C - intermediate result: condenser/evaporator temp diff
        Real64 AvailNomCapRat;           // intermediate result: available nominal capacity ratio
        Real64 FullLoadPowerRat;         // intermediate result: full load power ratio
        Real64 PartLoadRat;              // part load ratio for efficiency calculation
        Real64 OperPartLoadRat;          // Actual Operating PLR
        Real64 TempLowLimitEout;         // C - Evaporator low temp. limit cut off
        Real64 EvapMassFlowRateMax;      // Max Design Evaporator Mass Flow Rate converted from Volume Flow Rate
        int EvapInletNode;               // evaporator inlet node number, water side
        int EvapOutletNode;              // evaporator outlet node number, water side
        int CondInletNode;               // condenser inlet node number, water side
        int CondOutletNode;              // condenser outlet node number, water side
        Real64 FRAC;
        int PlantLoopNum;
        int LoopNum;
        int LoopSideNum;
        int BranchNum;
        int CompNum;
        Real64 CurrentEndTime;                 // end time of time step for current simulation time step
        std::string OutputChar;         // character string for warning messages
        Real64 Cp;                             // local for fluid specif heat, for evaporator
        Real64 CpCond;                         // local for fluid specif heat, for condenser

        // set module level inlet and outlet nodes
        modEvapMassFlowRate = 0.0;
        modCondMassFlowRate = 0.0;
        modPower = 0.0;
        modEnergy = 0.0;
        modQCondenser = 0.0;
        modQEvaporator = 0.0;
        modCondenserEnergy = 0.0;
        modEvaporatorEnergy = 0.0;
        modQHeatRecovered = 0.0;
        EvapInletNode = ElectricChiller(ChillNum).Base.EvapInletNodeNum;
        EvapOutletNode = ElectricChiller(ChillNum).Base.EvapOutletNodeNum;
        CondInletNode = ElectricChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = ElectricChiller(ChillNum).Base.CondOutletNodeNum;
        FRAC = 1.0;
        LoopNum = ElectricChiller(ChillNum).Base.CWLoopNum;
        LoopSideNum = ElectricChiller(ChillNum).Base.CWLoopSideNum;
        BranchNum = ElectricChiller(ChillNum).Base.CWBranchNum;
        CompNum = ElectricChiller(ChillNum).Base.CWCompNum;
        _EvapInletTemp = Node(EvapInletNode).Temp;

        //   calculate end time of current time step
        CurrentEndTime = DataGlobals::CurrentTime + DataHVACGlobals::SysTimeElapsed;

        //   Print warning messages only when valid and only for the first ocurrance. Let summary provide statistics.
        //   Wait for next time step to print warnings. If simulation iterates, print out
        //   the warning for the last iteration only. Must wait for next time step to accomplish this.
        //   If a warning occurs and the simulation down shifts, the warning is not valid.
        if (CurrentEndTime > ElectricChiller(ChillNum).Base.CurrentEndTimeLast && DataHVACGlobals::TimeStepSys >= ElectricChiller(ChillNum).Base.TimeStepSysLast) {
            if (ElectricChiller(ChillNum).Base.PrintMessage) {
                ++ElectricChiller(ChillNum).Base.MsgErrorCount;
                //       Show single warning and pass additional info to ShowRecurringWarningErrorAtEnd
                if (ElectricChiller(ChillNum).Base.MsgErrorCount < 2) {
                    ShowWarningError(ElectricChiller(ChillNum).Base.MsgBuffer1 + '.');
                    ShowContinueError(ElectricChiller(ChillNum).Base.MsgBuffer2);
                } else {
                    ShowRecurringWarningErrorAtEnd(ElectricChiller(ChillNum).Base.MsgBuffer1 + " error continues.",
                                                   ElectricChiller(ChillNum).Base.ErrCount1,
                                                   ElectricChiller(ChillNum).Base.MsgDataLast,
                                                   ElectricChiller(ChillNum).Base.MsgDataLast,
                                                   _,
                                                   "[C]",
                                                   "[C]");
                }
            }
        }

        //   save last system time step and last end time of current time step (used to determine if warning is valid)
        ElectricChiller(ChillNum).Base.TimeStepSysLast = DataHVACGlobals::TimeStepSys;
        ElectricChiller(ChillNum).Base.CurrentEndTimeLast = CurrentEndTime;

        // If no loop demand or chiller OFF, return
        // If Chiller load is 0 or chiller is not running then leave the subroutine.
        if (MyLoad >= 0.0 || !RunFlag) {
            // call for zero flow before leaving
            if (EquipFlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive || DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 1) {
                modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            } else {
                modEvapMassFlowRate = 0.0;
                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     ElectricChiller(ChillNum).Base.CWLoopNum,
                                     ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                     ElectricChiller(ChillNum).Base.CWBranchNum,
                                     ElectricChiller(ChillNum).Base.CWCompNum);
            }
            if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
                if (DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum)
                        .LoopSide(ElectricChiller(ChillNum).Base.CDLoopSideNum)
                        .Branch(ElectricChiller(ChillNum).Base.CDBranchNum)
                        .Comp(ElectricChiller(ChillNum).Base.CDCompNum)
                        .FlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive) {
                    modCondMassFlowRate = Node(CondInletNode).MassFlowRate;
                } else {
                    modCondMassFlowRate = 0.0;
                    PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                         CondInletNode,
                                         CondOutletNode,
                                         ElectricChiller(ChillNum).Base.CDLoopNum,
                                         ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                         ElectricChiller(ChillNum).Base.CDBranchNum,
                                         ElectricChiller(ChillNum).Base.CDCompNum);
                }
            }

            if (ElectricChiller(ChillNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(ElectricChiller(ChillNum).Base.BasinHeaterPowerFTempDiff,
                                     ElectricChiller(ChillNum).Base.BasinHeaterSchedulePtr,
                                     ElectricChiller(ChillNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }
            ElectricChiller(ChillNum).Base.PrintMessage = false;
            return;
        }

        // If not air or evap cooled then set to the condenser node that is attached to a cooling tower
        _CondInletTemp = Node(CondInletNode).Temp;

        // Set mass flow rates
        if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
            modCondMassFlowRate = ElectricChiller(ChillNum).Base.CondMassFlowRateMax;
            PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                 CondInletNode,
                                 CondOutletNode,
                                 ElectricChiller(ChillNum).Base.CDLoopNum,
                                 ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                 ElectricChiller(ChillNum).Base.CDBranchNum,
                                 ElectricChiller(ChillNum).Base.CDCompNum);
            PlantUtilities::PullCompInterconnectTrigger(ElectricChiller(ChillNum).Base.CWLoopNum,
                                        ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                        ElectricChiller(ChillNum).Base.CWBranchNum,
                                        ElectricChiller(ChillNum).Base.CWCompNum,
                                        ElectricChiller(ChillNum).Base.CondMassFlowIndex,
                                        ElectricChiller(ChillNum).Base.CDLoopNum,
                                        ElectricChiller(ChillNum).Base.CDLoopSideNum,
                                        DataPlant::CriteriaType_MassFlowRate,
                                                        modCondMassFlowRate);
            if (modCondMassFlowRate < DataBranchAirLoopPlant::MassFlowTolerance) return;
        }

        //  LOAD LOCAL VARIABLES FROM DATA STRUCTURE (for code readability)
        auto const &CapacityRat(ElectricChiller(ChillNum).CapRatCoef);
        auto const &PowerRat(ElectricChiller(ChillNum).PowerRatCoef);
        auto const &FullLoadFactor(ElectricChiller(ChillNum).FullLoadCoef);
        MinPartLoadRat = ElectricChiller(ChillNum).MinPartLoadRat;
        PartLoadRat = MinPartLoadRat;
        MaxPartLoadRat = ElectricChiller(ChillNum).MaxPartLoadRat;
        TempCondInDesign = ElectricChiller(ChillNum).TempDesCondIn;
        TempRiseRat = ElectricChiller(ChillNum).TempRiseCoef;
        TempEvapOutDesign = ElectricChiller(ChillNum).TempDesEvapOut;
        ChillerNomCap = ElectricChiller(ChillNum).Base.NomCap;
        RatedCOP = ElectricChiller(ChillNum).Base.COP;
        TempEvapOut = Node(ElectricChiller(ChillNum).Base.EvapOutletNodeNum).Temp;
        TempLowLimitEout = ElectricChiller(ChillNum).TempLowLimitEvapOut;
        EvapMassFlowRateMax = ElectricChiller(ChillNum).Base.EvapMassFlowRateMax;
        PlantLoopNum = ElectricChiller(ChillNum).Base.CWLoopNum;

        LoopNum = ElectricChiller(ChillNum).Base.CWLoopNum;
        LoopSideNum = ElectricChiller(ChillNum).Base.CWLoopSideNum;

        // If there is a fault of chiller fouling (zrp_Nov2016)
        if (ElectricChiller(ChillNum).Base.FaultyChillerFoulingFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = ElectricChiller(ChillNum).Base.FaultyChillerFoulingIndex;
            Real64 NomCap_ff = ChillerNomCap;
            Real64 RatedCOP_ff = RatedCOP;

            // calculate the Faulty Chiller Fouling Factor using fault information
            ElectricChiller(ChillNum).Base.FaultyChillerFoulingFactor = FaultsManager::FaultsChillerFouling(FaultIndex).CalFoulingFactor();

            // update the Chiller nominal capacity and COP at faulty cases
            ChillerNomCap = NomCap_ff * ElectricChiller(ChillNum).Base.FaultyChillerFoulingFactor;
            RatedCOP = RatedCOP_ff * ElectricChiller(ChillNum).Base.FaultyChillerFoulingFactor;
        }

        // initialize outlet air humidity ratio of air or evap cooled chillers
        modCondOutletHumRat = Node(CondInletNode).HumRat;

        if (ElectricChiller(ChillNum).Base.CondenserType == AirCooled) { // Condenser inlet temp = outdoor temp
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirDryBulb;
            //  Warn user if entering condenser temperature falls below 0C
            if (Node(CondInletNode).Temp < 0.0 && !WarmupFlag) {
                ElectricChiller(ChillNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                ElectricChiller(ChillNum).Base.MsgBuffer1 = "CalcElectricChillerModel - Chiller:Electric \"" + ElectricChiller(ChillNum).Base.Name +
                                                            "\" - Air Cooled Condenser Inlet Temperature below 0C";
                ElectricChiller(ChillNum).Base.MsgBuffer2 = "... Outdoor Dry-bulb Condition = " + OutputChar +
                                                            " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                                                            General::CreateSysTimeIntervalString();
                ElectricChiller(ChillNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                ElectricChiller(ChillNum).Base.PrintMessage = false;
            }
        } else if (ElectricChiller(ChillNum).Base.CondenserType == EvapCooled) { // Condenser inlet temp = (outdoor wet bulb)
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirWetBulb;
            //  line above assumes evaporation pushes condenser inlet air humidity ratio to saturation
            modCondOutletHumRat = Psychrometrics::PsyWFnTdbTwbPb(Node(CondInletNode).Temp, Node(CondInletNode).Temp, Node(CondInletNode).Press);
            //  Warn user if evap condenser wet bulb temperature falls below 10C
            if (Node(CondInletNode).Temp < 10.0 && !WarmupFlag) {
                ElectricChiller(ChillNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                ElectricChiller(ChillNum).Base.MsgBuffer1 = "CalcElectricChillerModel - Chiller:Electric \"" + ElectricChiller(ChillNum).Base.Name +
                                                            "\" - Evap Cooled Condenser Inlet Temperature below 10C";
                ElectricChiller(ChillNum).Base.MsgBuffer2 = "... Outdoor Wet-bulb Condition = " + OutputChar +
                                                            " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                                                            General::CreateSysTimeIntervalString();
                ElectricChiller(ChillNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                ElectricChiller(ChillNum).Base.PrintMessage = false;
            }
        } // End of the Air Cooled/Evap Cooled Logic block

        _CondInletTemp = Node(CondInletNode).Temp;

        // correct inlet temperature if using heat recovery
        if (ElectricChiller(ChillNum).HeatRecActive) {
            if ((ElectricChillerReport(ChillNum).QHeatRecovery + ElectricChillerReport(ChillNum).Base.QCond) > 0.0) {
                modAvgCondSinkTemp = (ElectricChillerReport(ChillNum).QHeatRecovery * ElectricChillerReport(ChillNum).HeatRecInletTemp +
                                   ElectricChillerReport(ChillNum).Base.QCond * ElectricChillerReport(ChillNum).Base.CondInletTemp) /
                                  (ElectricChillerReport(ChillNum).QHeatRecovery + ElectricChillerReport(ChillNum).Base.QCond);
            } else {
                modAvgCondSinkTemp = _CondInletTemp;
            }
        } else {
            modAvgCondSinkTemp = _CondInletTemp;
        }

        // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
        if (ElectricChiller(ChillNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = ElectricChiller(ChillNum).Base.FaultyChillerSWTIndex;
            Real64 EvapOutletTemp_ff = TempEvapOut;

            // calculate the sensor offset using fault information
            ElectricChiller(ChillNum).Base.FaultyChillerSWTOffset = FaultsManager::FaultsChillerSWTSensor(FaultIndex).CalFaultOffsetAct();
            // update the TempEvapOut
            TempEvapOut = max(ElectricChiller(ChillNum).TempLowLimitEvapOut,
                              min(Node(EvapInletNode).Temp, EvapOutletTemp_ff - ElectricChiller(ChillNum).Base.FaultyChillerSWTOffset));
            ElectricChiller(ChillNum).Base.FaultyChillerSWTOffset = EvapOutletTemp_ff - TempEvapOut;
        }

        // Calculate chiller performance from this set of performance equations.
        //  from BLAST...Z=(TECONDW-ADJTC(1))/ADJTC(2)-(TLCHLRW-ADJTC(3))

        DeltaTemp = (modAvgCondSinkTemp - TempCondInDesign) / TempRiseRat - (TempEvapOut - TempEvapOutDesign);

        // model should have bounds on DeltaTemp and check them (also needs engineering ref content)
        //  from BLAST...RCAV=RCAVC(1)+RCAVC(2)*Z+RCAVC(3)*Z**2
        AvailNomCapRat = CapacityRat(1) + CapacityRat(2) * DeltaTemp + CapacityRat(3) * pow_2(DeltaTemp);

        AvailChillerCap = ChillerNomCap * AvailNomCapRat;

        // from BLAST...G=ADJEC(1)+ADJEC(2)*RCAV+ADJEC(3)*RCAV**2.
        FullLoadPowerRat = PowerRat(1) + PowerRat(2) * AvailNomCapRat + PowerRat(3) * pow_2(AvailNomCapRat);

        //  from BLAST...RCLOAD=AMAX1(MINCHFR(I,IPLCTR),AMIN1(CHLRLOAD(I)/CHLROCAP(I) &
        //         /RCAV,MAXCHFR(I,IPLCTR)))

        // Calculate the PLR. When there is Min PLR and the load is less than Min PLR then the Frac Full load Power
        // is calculated at Min PLR, while all other calculations are based on the actual PLR. So in that case once
        // FracFullLoadPower is calculated the PLR should be recalculated
        if (AvailChillerCap > 0.0) {
            PartLoadRat = max(MinPartLoadRat, min(std::abs(MyLoad) / AvailChillerCap, MaxPartLoadRat));
        }

        // from BLAST...RPOWER=RPWRC(1)+RPWRC(2)*RCLOAD+RPWRC(3)*RCLOAD**2
        FracFullLoadPower = FullLoadFactor(1) + FullLoadFactor(2) * PartLoadRat + FullLoadFactor(3) * pow_2(PartLoadRat);

        // If the PLR is less than Min PLR calculate the actual PLR for calculations. The power will then adjust for
        // the cycling.
        if (AvailChillerCap > 0.0) {
            if (std::abs(MyLoad) / AvailChillerCap < MinPartLoadRat) {
                OperPartLoadRat = std::abs(MyLoad) / AvailChillerCap;
            } else {
                OperPartLoadRat = PartLoadRat;
            }
        } else {
            OperPartLoadRat = 0.0;
        }

        Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidName,
                                   Node(EvapInletNode).Temp,
                                   DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);

        // If FlowLock is True, the new resolved mdot is used to update Power, QEvap, Qcond, and
        // condenser side outlet temperature.
        if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0) {

            // ElectricChiller(ChillNum)%PossibleSubcooling = .FALSE.
            // PossibleSubcooling = .NOT. PlantLoop(PlantLoopNum)%TempSetPtCtrl
            ElectricChiller(ChillNum).Base.PossibleSubcooling =
                !(DataPlant::PlantLoop(PlantLoopNum).LoopSide(LoopSideNum).Branch(BranchNum).Comp(CompNum).CurOpSchemeType ==
                  DataPlant::CompSetPtBasedSchemeType);
            modQEvaporator = AvailChillerCap * OperPartLoadRat;
            if (OperPartLoadRat < MinPartLoadRat) {
                FRAC = min(1.0, (OperPartLoadRat / MinPartLoadRat));
            } else {
                FRAC = 1.0;
            }
            modPower = FracFullLoadPower * FullLoadPowerRat * AvailChillerCap / RatedCOP * FRAC;

            // Either set the flow to the Constant value or caluclate the flow for the variable volume
            if ((ElectricChiller(ChillNum).Base.FlowMode == ConstantFlow) || (ElectricChiller(ChillNum).Base.FlowMode == NotModulated)) {

                // Start by assuming max (design) flow
                modEvapMassFlowRate = EvapMassFlowRateMax;
                // Use SetComponentFlowRate to decide actual flow
                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     ElectricChiller(ChillNum).Base.CWLoopNum,
                                     ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                     ElectricChiller(ChillNum).Base.CWBranchNum,
                                     ElectricChiller(ChillNum).Base.CWCompNum);
                // Evaluate delta temp based on actual flow rate
                if (modEvapMassFlowRate != 0.0) {
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                } else {
                    EvapDeltaTemp = 0.0;
                }
                // Evaluate outlet temp based on delta
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;

            } else if (ElectricChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) {

                // Calculate the Delta Temp from the inlet temp to the chiller outlet setpoint
                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        EvapDeltaTemp = Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPoint;
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        EvapDeltaTemp = Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPointHi;
                    }
                }

                if (EvapDeltaTemp != 0.0) {

                    // Calculate desired flow to request based on load
                    modEvapMassFlowRate = std::abs(modQEvaporator / Cp / EvapDeltaTemp);
                    // Check to see if the Maximum is exceeded, if so set to maximum
                    if ((modEvapMassFlowRate - EvapMassFlowRateMax) > DataBranchAirLoopPlant::MassFlowTolerance) ElectricChiller(ChillNum).Base.PossibleSubcooling = true;
                    modEvapMassFlowRate = min(EvapMassFlowRateMax, modEvapMassFlowRate);
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         ElectricChiller(ChillNum).Base.CWLoopNum,
                                         ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                         ElectricChiller(ChillNum).Base.CWBranchNum,
                                         ElectricChiller(ChillNum).Base.CWCompNum);
                    {
                        auto const SELECT_CASE_var(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CWLoopNum).LoopDemandCalcScheme);
                        if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPoint;
                        } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPointHi;
                        }
                    }

                } else {

                    // Try to request zero flow
                    modEvapMassFlowRate = 0.0;
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         ElectricChiller(ChillNum).Base.CWLoopNum,
                                         ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                         ElectricChiller(ChillNum).Base.CWBranchNum,
                                         ElectricChiller(ChillNum).Base.CWCompNum);
                    // No deltaT since component is not running
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }

            } // End of Constant Variable Flow If Block

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (ElectricChiller(ChillNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = ElectricChiller(ChillNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = (ElectricChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated);
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        ElectricChiller(ChillNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                PartLoadRat = (AvailChillerCap > 0.0) ? (modQEvaporator / AvailChillerCap) : 0.0;
                PartLoadRat = max(0.0, min(PartLoadRat, MaxPartLoadRat));
                // ChillerPartLoadRatio = PartLoadRat;
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

        } else { // If FlowLock is True

            modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                 EvapInletNode,
                                 EvapOutletNode,
                                 ElectricChiller(ChillNum).Base.CWLoopNum,
                                 ElectricChiller(ChillNum).Base.CWLoopSideNum,
                                 ElectricChiller(ChillNum).Base.CWBranchNum,
                                 ElectricChiller(ChillNum).Base.CWCompNum);

            //       Some other component set the flow to 0. No reason to continue with calculations.
            if (modEvapMassFlowRate == 0.0) {
                MyLoad = 0.0;
                if (ElectricChiller(ChillNum).Base.CondenserType == EvapCooled) {
                    CalcBasinHeaterPower(ElectricChiller(ChillNum).Base.BasinHeaterPowerFTempDiff,
                                         ElectricChiller(ChillNum).Base.BasinHeaterSchedulePtr,
                                         ElectricChiller(ChillNum).Base.BasinHeaterSetPointTemp,
                                         modBasinHeaterPower);
                }
                ElectricChiller(ChillNum).Base.PrintMessage = false;
                return;
            }
            // Flow resolver might have given less flow or control scheme have provided more load, which may
            // result in subcooling.
            if (ElectricChiller(ChillNum).Base.PossibleSubcooling) {
                modQEvaporator = std::abs(MyLoad);
                EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
            } else { // No subcooling in this case.No recalculation required.Still need to check chiller low temp limit

                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        if ((ElectricChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) ||
                            (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).Branch(BranchNum).Comp(CompNum).CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                            (Node(EvapOutletNode).TempSetPoint != SensedNodeFlagValue)) {
                            TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPoint;
                        } else {
                            TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPoint;
                        }
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        if ((ElectricChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) ||
                            (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).Branch(BranchNum).Comp(CompNum).CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                            (Node(EvapOutletNode).TempSetPointHi != SensedNodeFlagValue)) {
                            TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPointHi;
                        } else {
                            TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPointHi;
                        }
                    }
                }
                EvapDeltaTemp = Node(EvapInletNode).Temp - TempEvapOutSetPoint;
                modQEvaporator = std::abs(modEvapMassFlowRate * Cp * EvapDeltaTemp);
                modEvapOutletTemp = TempEvapOutSetPoint;
            }
            // Check that the Evap outlet temp honors both plant loop temp low limit and also the chiller low limit
            if (modEvapOutletTemp < TempLowLimitEout) {
                if ((Node(EvapInletNode).Temp - TempLowLimitEout) > DeltaTempTol) {
                    modEvapOutletTemp = TempLowLimitEout;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }
            if (modEvapOutletTemp < Node(EvapOutletNode).TempMin) {
                if ((Node(EvapInletNode).Temp - Node(EvapOutletNode).TempMin) > DeltaTempTol) {
                    modEvapOutletTemp = Node(EvapOutletNode).TempMin;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }

            // If load exceeds the distributed load set to the distributed load
            if (modQEvaporator > std::abs(MyLoad)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = std::abs(MyLoad);
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (ElectricChiller(ChillNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = ElectricChiller(ChillNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = false;
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        ElectricChiller(ChillNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

            // Checks QEvaporator on the basis of the machine limits.
            if (modQEvaporator > (AvailChillerCap * MaxPartLoadRat)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = AvailChillerCap * OperPartLoadRat;
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            if (OperPartLoadRat < MinPartLoadRat) {
                FRAC = min(1.0, (OperPartLoadRat / MinPartLoadRat));
            } else {
                FRAC = 1.0;
            }

            // set the module level variable used for reporting FRAC
            modChillerCyclingRatio = FRAC;

            // Chiller is false loading below PLR = minimum unloading ratio, find PLR used for energy calculation
            modPower = FracFullLoadPower * FullLoadPowerRat * AvailChillerCap / RatedCOP * FRAC;

            if (modEvapMassFlowRate == 0.0) {
                modQEvaporator = 0.0;
                modEvapOutletTemp = Node(EvapInletNode).Temp;
                modPower = 0.0;
                ElectricChiller(ChillNum).Base.PrintMessage = false;
            }
            if (modQEvaporator == 0.0 && ElectricChiller(ChillNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(ElectricChiller(ChillNum).Base.BasinHeaterPowerFTempDiff,
                                     ElectricChiller(ChillNum).Base.BasinHeaterSchedulePtr,
                                     ElectricChiller(ChillNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }
        } // This is the end of the FlowLock Block

        // QCondenser is calculated the same for each type, but the power consumption should be different
        //  depending on the performance coefficients used for the chiller model.
        modQCondenser = modPower + modQEvaporator;

        if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
            if (modCondMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                // If Heat Recovery specified for this vapor compression chiller, then Qcondenser will be adjusted by this subroutine
                if (ElectricChiller(ChillNum).HeatRecActive)
                    CalcElectricChillerHeatRecovery(ChillNum, modQCondenser, modCondMassFlowRate, _CondInletTemp, modQHeatRecovered);
                CpCond = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidName,
                                                                _CondInletTemp,
                                               DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                               RoutineName);
                modCondOutletTemp = modQCondenser / modCondMassFlowRate / CpCond + _CondInletTemp;
            } else {
                ShowSevereError("CalcElectricChillerModel: Condenser flow = 0, for ElectricChiller=" + ElectricChiller(ChillNum).Base.Name);
                ShowContinueErrorTimeStamp("");
            }
        } else { // Air Cooled or Evap Cooled

            if (modQCondenser > 0.0) {
                modCondMassFlowRate = ElectricChiller(ChillNum).Base.CondMassFlowRateMax * OperPartLoadRat;
            } else {
                modCondMassFlowRate = 0.0;
            }

            // If Heat Recovery specified for this vapor compression chiller, then Qcondenser will be adjusted by this subroutine
            if (ElectricChiller(ChillNum).HeatRecActive)
                CalcElectricChillerHeatRecovery(ChillNum, modQCondenser, modCondMassFlowRate, _CondInletTemp, modQHeatRecovered);
            if (modCondMassFlowRate > 0.0) {
                CpCond = Psychrometrics::PsyCpAirFnWTdb(Node(CondInletNode).HumRat, _CondInletTemp);
                modCondOutletTemp = _CondInletTemp + modQCondenser / modCondMassFlowRate / CpCond;
            } else {
                modCondOutletTemp = _CondInletTemp;
            }
        }

        // Calculate Energy
        modCondenserEnergy = modQCondenser * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEnergy = modPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEvaporatorEnergy = modQEvaporator * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        // check for problems BG 9/12/06 (deal with observed negative energy results)
        if (modEnergy < 0.0) { // there is a serious problem

            if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
                // first check for run away condenser loop temps (only reason yet to be observed for this?)
                if (_CondInletTemp > 70.0) {
                    ShowSevereError("CalcElectricChillerModel: Condenser loop inlet temperatures over 70.0 C for ElectricChiller=" +
                                    ElectricChiller(ChillNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Condenser loop water temperatures are too high at" + RoundSigDigits(_CondInletTemp, 2));
                    ShowContinueError("Check input for condenser plant loop, especially cooling tower");
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));

                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            if (!WarmupFlag) {
                if (AvailNomCapRat < 0.0) { // apparently the real reason energy goes negative
                    ShowSevereError("CalcElectricChillerModel: Capacity ratio below zero for ElectricChiller=" + ElectricChiller(ChillNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Check input for Capacity Ratio Curve");
                    ShowContinueError("Condenser inlet temperature: " + RoundSigDigits(_CondInletTemp, 2));
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));
                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            // If makes it here, set limits, chiller can't have negative energy/power
            // proceeding silently for now but may want to throw error here
            modPower = 0.0;
            modEnergy = 0.0;
        }
    }

    void CalcEngineDrivenChillerModel(int &ChillerNum,        // chiller number
                                      Real64 &MyLoad,         // operating load
                                      bool const RunFlag,     // TRUE when chiller operating
                                      int const EquipFlowCtrl // Flow control mode for the equipment
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Dan Fisher / Brandon Anderson
        //       DATE WRITTEN   Sept. 2000
        //       MODIFIED       Feb. 2010, Chandan Sharma, FSEC. Added basin heater
        //                      Jun. 2016, Rongpeng Zhang, LBNL. Applied the chiller supply water temperature sensor fault model
        //                      Nov. 2016, Rongpeng Zhang, LBNL. Added Fouling Chiller fault
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // simulate a vapor compression chiller using the EngineDriven model

        // METHODOLOGY EMPLOYED:
        // curve fit of performance data:

        // REFERENCES:
        // 1. BLAST Users Manual
        // 2. CHILLER User Manual

        // Locals
        Real64 _EvapInletTemp; // C - evaporator inlet temperature, water side
        Real64 _CondInletTemp; // C - condenser inlet temperature, water side

        Real64 const ExhaustCP(1.047);    // Exhaust Gas Specific Heat (J/kg-K)
        Real64 const ReferenceTemp(25.0); // Reference temperature by which lower heating
        // value is reported.  This should be subtracted
        // off of when calculated exhaust energies.
        static ObjexxFCL::gio::Fmt OutputFormat("(F6.2)");
        static std::string const RoutineName("CalcEngineDrivenChillerModel");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 MinPartLoadRat;           // min allowed operating frac full load
        Real64 MaxPartLoadRat;           // max allowed operating frac full load
        Real64 TempCondIn;               // C - (EngineDriven ADJTC(1)The design secondary loop fluid
        Real64 TempCondInDesign;         // C - (EngineDriven ADJTC(1)The design secondary loop fluid
        Real64 TempRiseRat;              // intermediate result:  temperature rise ratio
        Real64 TempEvapOut;              // C - evaporator outlet temperature, water side
        Real64 TempEvapOutSetPoint(0.0); // C - evaporator outlet temperature setpoint
        Real64 TempEvapOutDesign;        // design evaporator outlet temperature, water side
        Real64 ChillerNomCap;            // chiller nominal capacity
        Real64 AvailChillerCap;          // chiller available capacity
        Real64 COP;                      // coefficient of performance
        Real64 FracFullLoadPower;        // fraction of full load power
        Real64 EvapDeltaTemp(0.0);       // C - evaporator temperature difference, water side
        Real64 DeltaTemp;                // C - intermediate result: condenser/evaporator temp diff
        Real64 AvailNomCapRat;           // intermediate result: available nominal capacity ratio
        Real64 FullLoadPowerRat;         // intermediate result: full load power ratio
        Real64 PartLoadRat(0.0);         // part load ratio for efficiency
        Real64 OperPartLoadRat;          // Actual operating PLR
        int EvapInletNode;               // evaporator inlet node number, water side
        int EvapOutletNode;              // evaporator outlet node number, water side
        int CondInletNode;               // condenser inlet node number, water side
        int CondOutletNode;              // condenser outlet node number, water side
        Real64 EvapMassFlowRateMax;      // Max Design Evaporator Mass Flow Rate converted from Volume Flow Rate
        Real64 TempLowLimitEout;         // C - Evaporator low temp. limit cut off
        Real64 FRAC;
        int LoopNum;
        int LoopSideNum;
        Real64 CurrentEndTime;                 // end time of time step for current simulation time step
        std::string OutputChar;         // character string for warning messages
        Real64 Cp;                             // local for fluid specif heat, for evaporator
        Real64 CpCond;                         // local for fluid specif heat, for condenser

        // Special variables for EngineDriven Chiller
        Real64 MaxExhaustperPowerOutput; // curve fit parameter
        Real64 ClngLoadFuelRat;          // (RELDC) Ratio of Shaft Power to Fuel Energy Input
        Real64 RecJacHeattoFuelRat;      // (RJACDC) Ratio of Recoverable Jacket Heat to Fuel Energy Input
        Real64 RecLubeHeattoFuelRat;     // (RLUBDC) Ratio of Recoverable Lube Oil Heat to Fuel Energy Input
        Real64 TotExhausttoFuelRat;      // (REXDC) Total Exhaust Energy Input to Fuel Energy Input
        Real64 TotalExhaustEnergy;
        Real64 ExhaustTemp;    // (TEX) Exhaust Gas Temp
        Real64 ExhaustGasFlow; // exhaust gas mass flow rate
        Real64 DesignMinExitGasTemp;
        Real64 UA;        // (UACDC) exhaust gas Heat Exchanger UA
        Real64 HeatRecCp; // Specific Heat of the Heat Recovery Fluid (J/kg-K)
        Real64 EngineDrivenFuelEnergy;
        Real64 HeatRecRatio; // When Max Temp is reached the amount of recovered heat has to be reduced.

        // set module level inlet and outlet nodes
        modEvapMassFlowRate = 0.0;
        modCondMassFlowRate = 0.0;
        modPower = 0.0;
        modQCondenser = 0.0;
        modQEvaporator = 0.0;
        modEnergy = 0.0;
        modCondenserEnergy = 0.0;
        modEvaporatorEnergy = 0.0;
        HeatRecCp = 0.0;
        EngineDrivenChiller(ChillerNum).modHeatRecMdotActual = 0.0;
        EngineDrivenChiller(ChillerNum).modQTotalHeatRecovered = 0.0;
        EngineDrivenChiller(ChillerNum).modQJacketRecovered = 0.0;
        EngineDrivenChiller(ChillerNum).modQLubeOilRecovered = 0.0;
        EngineDrivenChiller(ChillerNum).modQExhaustRecovered = 0.0;
        EngineDrivenFuelEnergy = 0.0;
        EngineDrivenChiller(ChillerNum).modFuelEnergyUseRate = 0.0;
        EngineDrivenChiller(ChillerNum).modTotalHeatEnergyRec = 0.0;
        EngineDrivenChiller(ChillerNum).modJacketEnergyRec = 0.0;
        EngineDrivenChiller(ChillerNum).modLubeOilEnergyRec = 0.0;
        EngineDrivenChiller(ChillerNum).modExhaustEnergyRec = 0.0;
        EngineDrivenChiller(ChillerNum).modFuelEnergy = 0.0;
        EngineDrivenChiller(ChillerNum).modFuelMdot = 0.0;
        EngineDrivenChiller(ChillerNum).modExhaustStackTemp = 0.0;
        FRAC = 1.0;

        if (EngineDrivenChiller(ChillerNum).HeatRecActive) {
            EngineDrivenChiller(ChillerNum).modHeatRecInletTemp = Node(EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum).Temp;
            modHeatRecOutletTemp = Node(EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum).Temp;
        }

        EvapInletNode = EngineDrivenChiller(ChillerNum).Base.EvapInletNodeNum;
        EvapOutletNode = EngineDrivenChiller(ChillerNum).Base.EvapOutletNodeNum;
        CondInletNode = EngineDrivenChiller(ChillerNum).Base.CondInletNodeNum;
        CondOutletNode = EngineDrivenChiller(ChillerNum).Base.CondOutletNodeNum;
        LoopNum = EngineDrivenChiller(ChillerNum).Base.CWLoopNum;
        LoopSideNum = EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum;
        _EvapInletTemp = Node(EvapInletNode).Temp;

        //   calculate end time of current time step
        CurrentEndTime = DataGlobals::CurrentTime + DataHVACGlobals::SysTimeElapsed;

        //   Print warning messages only when valid and only for the first ocurrance. Let summary provide statistics.
        //   Wait for next time step to print warnings. If simulation iterates, print out
        //   the warning for the last iteration only. Must wait for next time step to accomplish this.
        //   If a warning occurs and the simulation down shifts, the warning is not valid.
        if (CurrentEndTime > EngineDrivenChiller(ChillerNum).Base.CurrentEndTimeLast && DataHVACGlobals::TimeStepSys >= EngineDrivenChiller(ChillerNum).Base.TimeStepSysLast) {
            if (EngineDrivenChiller(ChillerNum).Base.PrintMessage) {
                ++EngineDrivenChiller(ChillerNum).Base.MsgErrorCount;
                //     Show single warning and pass additional info to ShowRecurringWarningErrorAtEnd
                if (EngineDrivenChiller(ChillerNum).Base.MsgErrorCount < 2) {
                    ShowWarningError(EngineDrivenChiller(ChillerNum).Base.MsgBuffer1 + '.');
                    ShowContinueError(EngineDrivenChiller(ChillerNum).Base.MsgBuffer2);
                } else {
                    ShowRecurringWarningErrorAtEnd(EngineDrivenChiller(ChillerNum).Base.MsgBuffer1 + " error continues.",
                                                   EngineDrivenChiller(ChillerNum).Base.ErrCount1,
                                                   EngineDrivenChiller(ChillerNum).Base.MsgDataLast,
                                                   EngineDrivenChiller(ChillerNum).Base.MsgDataLast,
                                                   _,
                                                   "[C]",
                                                   "[C]");
                }
            }
        }

        // save last system time step and last end time of current time step (used to determine if warning is valid)
        EngineDrivenChiller(ChillerNum).Base.TimeStepSysLast = DataHVACGlobals::TimeStepSys;
        EngineDrivenChiller(ChillerNum).Base.CurrentEndTimeLast = CurrentEndTime;

        // If Chiller load is 0 or chiller is not running then leave the subroutine.
        if (MyLoad >= 0.0 || !RunFlag) {
            if (EquipFlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive || DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 1) {
                modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            } else {
                modEvapMassFlowRate = 0.0;

                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     EngineDrivenChiller(ChillerNum).Base.CWLoopNum,
                                     EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum,
                                     EngineDrivenChiller(ChillerNum).Base.CWBranchNum,
                                     EngineDrivenChiller(ChillerNum).Base.CWCompNum);
            }

            if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                if (DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CDLoopNum)
                        .LoopSide(EngineDrivenChiller(ChillerNum).Base.CDLoopSideNum)
                        .Branch(EngineDrivenChiller(ChillerNum).Base.CDBranchNum)
                        .Comp(EngineDrivenChiller(ChillerNum).Base.CDCompNum)
                        .FlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive) {
                    modCondMassFlowRate = Node(CondInletNode).MassFlowRate;
                } else {
                    modCondMassFlowRate = 0.0;
                    PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                         CondInletNode,
                                         CondOutletNode,
                                         EngineDrivenChiller(ChillerNum).Base.CDLoopNum,
                                         EngineDrivenChiller(ChillerNum).Base.CDLoopSideNum,
                                         EngineDrivenChiller(ChillerNum).Base.CDBranchNum,
                                         EngineDrivenChiller(ChillerNum).Base.CDCompNum);
                }
            }

            if (EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(EngineDrivenChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff,
                                     EngineDrivenChiller(ChillerNum).Base.BasinHeaterSchedulePtr,
                                     EngineDrivenChiller(ChillerNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }
            EngineDrivenChiller(ChillerNum).Base.PrintMessage = false;
            return;
        }

        if (EngineDrivenChiller(ChillerNum).Base.CondenserType == AirCooled) { // Condenser inlet temp = outdoor temp
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirDryBulb;
            //  Warn user if entering condenser temperature falls below 0C
            if (Node(CondInletNode).Temp < 0.0 && !WarmupFlag) {
                EngineDrivenChiller(ChillerNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                EngineDrivenChiller(ChillerNum).Base.MsgBuffer1 = "CalcEngineDrivenChillerModel - Chiller:EngineDriven \"" +
                                                                  EngineDrivenChiller(ChillerNum).Base.Name +
                                                                  "\" - Air Cooled Condenser Inlet Temperature below 0C";
                EngineDrivenChiller(ChillerNum).Base.MsgBuffer2 = "... Outdoor Dry-bulb Condition = " + OutputChar +
                                                                  " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                    General::CreateSysTimeIntervalString();
                EngineDrivenChiller(ChillerNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                EngineDrivenChiller(ChillerNum).Base.PrintMessage = false;
            }
        } else if (EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) { // Condenser inlet temp = (outdoor wet bulb)
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirWetBulb;
            //  Warn user if evap condenser wet bulb temperature falls below 10C
            if (Node(CondInletNode).Temp < 10.0 && !WarmupFlag) {
                EngineDrivenChiller(ChillerNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                EngineDrivenChiller(ChillerNum).Base.MsgBuffer1 = "CalcEngineDrivenChillerModel - Chiller:EngineDriven \"" +
                                                                  EngineDrivenChiller(ChillerNum).Base.Name +
                                                                  "\" - Evap Cooled Condenser Inlet Temperature below 10C";
                EngineDrivenChiller(ChillerNum).Base.MsgBuffer2 = "... Outdoor Wet-bulb Condition = " + OutputChar +
                                                                  " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                    General::CreateSysTimeIntervalString();
                EngineDrivenChiller(ChillerNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                EngineDrivenChiller(ChillerNum).Base.PrintMessage = false;
            }
        } // End of the Air Cooled/Evap Cooled Logic block

        // If not air or evap cooled then set to the condenser node that is attached to a cooling tower
        _CondInletTemp = Node(CondInletNode).Temp;

        // Set mass flow rates
        if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {
            modCondMassFlowRate = EngineDrivenChiller(ChillerNum).Base.CondMassFlowRateMax;
            PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                 CondInletNode,
                                 CondOutletNode,
                                 EngineDrivenChiller(ChillerNum).Base.CDLoopNum,
                                 EngineDrivenChiller(ChillerNum).Base.CDLoopSideNum,
                                 EngineDrivenChiller(ChillerNum).Base.CDBranchNum,
                                 EngineDrivenChiller(ChillerNum).Base.CDCompNum);
            PlantUtilities::PullCompInterconnectTrigger(EngineDrivenChiller(ChillerNum).Base.CWLoopNum,
                                        EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum,
                                        EngineDrivenChiller(ChillerNum).Base.CWBranchNum,
                                        EngineDrivenChiller(ChillerNum).Base.CWCompNum,
                                        EngineDrivenChiller(ChillerNum).Base.CondMassFlowIndex,
                                        EngineDrivenChiller(ChillerNum).Base.CDLoopNum,
                                        EngineDrivenChiller(ChillerNum).Base.CDLoopSideNum,
                                        DataPlant::CriteriaType_MassFlowRate,
                                                        modCondMassFlowRate);
            if (modCondMassFlowRate < DataBranchAirLoopPlant::MassFlowTolerance) return;
        }

        //  LOAD LOCAL VARIABLES FROM DATA STRUCTURE (for code readability)
        auto const &CapacityRat(EngineDrivenChiller(ChillerNum).CapRatCoef);
        auto const &PowerRat(EngineDrivenChiller(ChillerNum).PowerRatCoef);
        auto const &FullLoadFactor(EngineDrivenChiller(ChillerNum).FullLoadCoef);
        MinPartLoadRat = EngineDrivenChiller(ChillerNum).MinPartLoadRat;
        MaxPartLoadRat = EngineDrivenChiller(ChillerNum).MaxPartLoadRat;
        TempCondInDesign = EngineDrivenChiller(ChillerNum).TempDesCondIn;
        TempRiseRat = EngineDrivenChiller(ChillerNum).TempRiseCoef;
        TempEvapOutDesign = EngineDrivenChiller(ChillerNum).TempDesEvapOut;
        ChillerNomCap = EngineDrivenChiller(ChillerNum).Base.NomCap;
        COP = EngineDrivenChiller(ChillerNum).Base.COP;
        TempCondIn = Node(EngineDrivenChiller(ChillerNum).Base.CondInletNodeNum).Temp;
        TempEvapOut = Node(EngineDrivenChiller(ChillerNum).Base.EvapOutletNodeNum).Temp;
        TempLowLimitEout = EngineDrivenChiller(ChillerNum).TempLowLimitEvapOut;
        MaxExhaustperPowerOutput = EngineDrivenChiller(ChillerNum).MaxExhaustperPowerOutput;
        LoopNum = EngineDrivenChiller(ChillerNum).Base.CWLoopNum;
        LoopSideNum = EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum;
        EvapMassFlowRateMax = EngineDrivenChiller(ChillerNum).Base.EvapMassFlowRateMax;

        // If there is a fault of chiller fouling (zrp_Nov2016)
        if (EngineDrivenChiller(ChillerNum).Base.FaultyChillerFoulingFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = EngineDrivenChiller(ChillerNum).Base.FaultyChillerFoulingIndex;
            Real64 NomCap_ff = ChillerNomCap;
            Real64 COP_ff = COP;

            // calculate the Faulty Chiller Fouling Factor using fault information
            EngineDrivenChiller(ChillerNum).Base.FaultyChillerFoulingFactor = FaultsManager::FaultsChillerFouling(FaultIndex).CalFoulingFactor();

            // update the Chiller nominal capacity and COP at faulty cases
            ChillerNomCap = NomCap_ff * EngineDrivenChiller(ChillerNum).Base.FaultyChillerFoulingFactor;
            COP = COP_ff * EngineDrivenChiller(ChillerNum).Base.FaultyChillerFoulingFactor;
        }

        // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
        if (EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTIndex;
            Real64 EvapOutletTemp_ff = TempEvapOut;

            // calculate the sensor offset using fault information
            EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTOffset = FaultsManager::FaultsChillerSWTSensor(FaultIndex).CalFaultOffsetAct();
            // update the TempEvapOut
            TempEvapOut = max(EngineDrivenChiller(ChillerNum).TempLowLimitEvapOut,
                              min(Node(EvapInletNode).Temp, EvapOutletTemp_ff - EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTOffset));
            EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTOffset = EvapOutletTemp_ff - TempEvapOut;
        }

        // Calculate chiller performance from this set of performance equations.
        //  from BLAST...Z=(TECONDW-ADJTC(1))/ADJTC(2)-(TLCHLRW-ADJTC(3))
        DeltaTemp = (TempCondIn - TempCondInDesign) / TempRiseRat - (TempEvapOut - TempEvapOutDesign);

        //  from BLAST...RCAV=RCAVC(1)+RCAVC(2)*Z+RCAVC(3)*Z**2
        AvailNomCapRat = CapacityRat(1) + CapacityRat(2) * DeltaTemp + CapacityRat(3) * pow_2(DeltaTemp);

        AvailChillerCap = ChillerNomCap * AvailNomCapRat;

        // from BLAST...G=ADJEC(1)+ADJEC(2)*RCAV+ADJEC(3)*RCAV**2.
        FullLoadPowerRat = PowerRat(1) + PowerRat(2) * AvailNomCapRat + PowerRat(3) * pow_2(AvailNomCapRat);

        //  from BLAST...RCLOAD=AMAX1(MINCHFR(I,IPLCTR),AMIN1(CHLRLOAD(I)/CHLROCAP(I) &
        //         /RCAV,MAXCHFR(I,IPLCTR)))
        if (AvailChillerCap > 0.0) {
            PartLoadRat = max(MinPartLoadRat, min(std::abs(MyLoad) / AvailChillerCap, MaxPartLoadRat));
        }
        // from BLAST...RPOWER=RPWRC(1)+RPWRC(2)*RCLOAD+RPWRC(3)*RCLOAD**2
        FracFullLoadPower = FullLoadFactor(1) + FullLoadFactor(2) * PartLoadRat + FullLoadFactor(3) * pow_2(PartLoadRat);

        if (AvailChillerCap > 0.0) {
            if (std::abs(MyLoad) / AvailChillerCap < MinPartLoadRat) {
                OperPartLoadRat = std::abs(MyLoad) / AvailChillerCap;
            } else {
                OperPartLoadRat = PartLoadRat;
            }
        } else {
            OperPartLoadRat = 0.0;
        }

        Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CWLoopNum).FluidName,
                                   Node(EvapInletNode).Temp,
                                   DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);

        // If FlowLock is True, the new resolved mdot is used to update Power, QEvap, Qcond, and
        // condenser side outlet temperature.
        if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0) {
            EngineDrivenChiller(ChillerNum).Base.PossibleSubcooling = false;
            modQEvaporator = AvailChillerCap * OperPartLoadRat;
            if (OperPartLoadRat < MinPartLoadRat) {
                FRAC = min(1.0, (OperPartLoadRat / MinPartLoadRat));
            } else {
                FRAC = 1.0;
            }
            modPower = FracFullLoadPower * FullLoadPowerRat * AvailChillerCap / COP * FRAC;

            // Either set the flow to the Constant value or caluclate the flow for the variable volume
            if ((EngineDrivenChiller(ChillerNum).Base.FlowMode == ConstantFlow) || (EngineDrivenChiller(ChillerNum).Base.FlowMode == NotModulated)) {
                // Start by assuming max (design) flow
                modEvapMassFlowRate = EvapMassFlowRateMax;
                // Use SetComponentFlowRate to decide actual flow
                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     EngineDrivenChiller(ChillerNum).Base.CWLoopNum,
                                     EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum,
                                     EngineDrivenChiller(ChillerNum).Base.CWBranchNum,
                                     EngineDrivenChiller(ChillerNum).Base.CWCompNum);
                // Evaluate delta temp based on actual flow rate
                if (modEvapMassFlowRate != 0.0) {
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                } else {
                    EvapDeltaTemp = 0.0;
                }
                // Evaluate outlet temp based on delta
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;

            } else if (EngineDrivenChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated) {

                // Calculate the Delta Temp from the inlet temp to the chiller outlet setpoint
                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CWLoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        EvapDeltaTemp = Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPoint;
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        EvapDeltaTemp = Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPointHi;
                    }
                }

                if (EvapDeltaTemp != 0.0) {
                    modEvapMassFlowRate = std::abs(modQEvaporator / Cp / EvapDeltaTemp);
                    if ((modEvapMassFlowRate - EvapMassFlowRateMax) > DataBranchAirLoopPlant::MassFlowTolerance) EngineDrivenChiller(ChillerNum).Base.PossibleSubcooling = true;
                    // Check to see if the Maximum is exceeded, if so set to maximum
                    modEvapMassFlowRate = min(EvapMassFlowRateMax, modEvapMassFlowRate);
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         EngineDrivenChiller(ChillerNum).Base.CWLoopNum,
                                         EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum,
                                         EngineDrivenChiller(ChillerNum).Base.CWBranchNum,
                                         EngineDrivenChiller(ChillerNum).Base.CWCompNum);
                    {
                        auto const SELECT_CASE_var(DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CWLoopNum).LoopDemandCalcScheme);
                        if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPoint;
                        } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPointHi;
                        }
                    }
                } else {
                    // Try to request zero flow
                    modEvapMassFlowRate = 0.0;
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         EngineDrivenChiller(ChillerNum).Base.CWLoopNum,
                                         EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum,
                                         EngineDrivenChiller(ChillerNum).Base.CWBranchNum,
                                         EngineDrivenChiller(ChillerNum).Base.CWCompNum);
                    // No deltaT since component is not running
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            } // End of Constant Variable Flow If Block

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = (EngineDrivenChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated);
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                PartLoadRat = (AvailChillerCap > 0.0) ? (modQEvaporator / AvailChillerCap) : 0.0;
                PartLoadRat = max(0.0, min(PartLoadRat, MaxPartLoadRat));
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

        } else { // If FlowLock is True

            modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                 EvapInletNode,
                                 EvapOutletNode,
                                 EngineDrivenChiller(ChillerNum).Base.CWLoopNum,
                                 EngineDrivenChiller(ChillerNum).Base.CWLoopSideNum,
                                 EngineDrivenChiller(ChillerNum).Base.CWBranchNum,
                                 EngineDrivenChiller(ChillerNum).Base.CWCompNum);
            // Some other component set the flow to 0. No reason to continue with calculations.
            if (modEvapMassFlowRate == 0.0) {
                MyLoad = 0.0;
                if (EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    CalcBasinHeaterPower(EngineDrivenChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff,
                                         EngineDrivenChiller(ChillerNum).Base.BasinHeaterSchedulePtr,
                                         EngineDrivenChiller(ChillerNum).Base.BasinHeaterSetPointTemp,
                                         modBasinHeaterPower);
                }
                EngineDrivenChiller(ChillerNum).Base.PrintMessage = false;
                return;
            }

            if (EngineDrivenChiller(ChillerNum).Base.PossibleSubcooling) {
                modQEvaporator = std::abs(MyLoad);
                EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                if (modEvapOutletTemp < Node(EvapOutletNode).TempMin) {
                    modEvapOutletTemp = Node(EvapOutletNode).TempMin;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = std::abs(modEvapMassFlowRate * Cp * EvapDeltaTemp);
                }
            } else { // No subcooling in this case.No recalculation required.Still need to check chiller low temp limit

                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        if ((EngineDrivenChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated) ||
                            (DataPlant::PlantLoop(LoopNum)
                                 .LoopSide(LoopSideNum)
                                 .Branch(EngineDrivenChiller(ChillerNum).Base.CWBranchNum)
                                 .Comp(EngineDrivenChiller(ChillerNum).Base.CWCompNum)
                                 .CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                            (Node(EvapOutletNode).TempSetPoint != SensedNodeFlagValue)) {
                            TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPoint;
                        } else {
                            TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPoint;
                        }
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        if ((EngineDrivenChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated) ||
                            (DataPlant::PlantLoop(LoopNum)
                                 .LoopSide(LoopSideNum)
                                 .Branch(EngineDrivenChiller(ChillerNum).Base.CWBranchNum)
                                 .Comp(EngineDrivenChiller(ChillerNum).Base.CWCompNum)
                                 .CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                            (Node(EvapOutletNode).TempSetPointHi != SensedNodeFlagValue)) {
                            TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPointHi;
                        } else {
                            TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPointHi;
                        }
                    }
                }
                EvapDeltaTemp = Node(EvapInletNode).Temp - TempEvapOutSetPoint;
                modQEvaporator = std::abs(modEvapMassFlowRate * Cp * EvapDeltaTemp);
                modEvapOutletTemp = TempEvapOutSetPoint;
            }
            // Check that the Evap outlet temp honors both plant loop temp low limit and also the chiller low limit
            if (modEvapOutletTemp < TempLowLimitEout) {
                if ((Node(EvapInletNode).Temp - TempLowLimitEout) > DeltaTempTol) {
                    modEvapOutletTemp = TempLowLimitEout;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }
            if (modEvapOutletTemp < Node(EvapOutletNode).TempMin) {
                if ((Node(EvapInletNode).Temp - Node(EvapOutletNode).TempMin) > DeltaTempTol) {
                    modEvapOutletTemp = Node(EvapOutletNode).TempMin;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }
            // If load exceeds the distributed load set to the distributed load
            if (modQEvaporator > std::abs(MyLoad)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = std::abs(MyLoad);
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = false;
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        EngineDrivenChiller(ChillerNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

            // Checks QEvaporator on the basis of the machine limits.
            if (modQEvaporator > (AvailChillerCap * MaxPartLoadRat)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = AvailChillerCap * OperPartLoadRat;
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            if (OperPartLoadRat < MinPartLoadRat) {
                FRAC = min(1.0, (OperPartLoadRat / MinPartLoadRat));
            } else {
                FRAC = 1.0;
            }

            // set the module level variable used for reporting FRAC
            modChillerCyclingRatio = FRAC;

            // Chiller is false loading below PLR = minimum unloading ratio, find PLR used for energy calculation
            modPower = FracFullLoadPower * FullLoadPowerRat * AvailChillerCap / COP * FRAC;

            if (modEvapMassFlowRate == 0.0) {
                modQEvaporator = 0.0;
                modEvapOutletTemp = Node(EvapInletNode).Temp;
                modPower = 0.0;
                EngineDrivenChiller(ChillerNum).Base.PrintMessage = false;
            }
            if (modQEvaporator == 0.0 && EngineDrivenChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(EngineDrivenChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff,
                                     EngineDrivenChiller(ChillerNum).Base.BasinHeaterSchedulePtr,
                                     EngineDrivenChiller(ChillerNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }
        } // This is the end of the FlowLock Block

        // Now determine Cooling
        // QCondenser is calculated the same for each type, but the power consumption should be different
        //  depending on the performance coefficients used for the chiller model.
        modQCondenser = modPower + modQEvaporator;

        if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {

            if (modCondMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                CpCond = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CDLoopNum).FluidName,
                                                                _CondInletTemp,
                                               DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).Base.CDLoopNum).FluidIndex,
                                               RoutineName);
                modCondOutletTemp = modQCondenser / modCondMassFlowRate / CpCond + _CondInletTemp;
            } else {
                ShowSevereError("CalcEngineDrivenChillerModel: Condenser flow = 0, for EngineDrivenChiller=" +
                                EngineDrivenChiller(ChillerNum).Base.Name);
                ShowContinueErrorTimeStamp("");
            }

        } else { // Air Cooled or Evap Cooled

            // don't care about outlet temp for Air-Cooled or Evap Cooled
            modCondOutletTemp = _CondInletTemp;
        }

        // EngineDriven Portion of the Engine Driven Chiller:

        // DETERMINE FUEL CONSUMED AND AVAILABLE WASTE HEAT

        // Use Curve fit to determine Fuel Energy Input.  For electric power generated in Watts, the fuel
        // energy input is calculated in J/s.  The PLBasedFuelInputCurve selects ratio of fuel flow (J/s)/cooling load (J/s).
        if (PartLoadRat == 0) {
            EngineDrivenFuelEnergy = 0.0;
        } else {
            PartLoadRat = max(MinPartLoadRat, PartLoadRat);
            ClngLoadFuelRat = CurveManager::CurveValue(EngineDrivenChiller(ChillerNum).ClngLoadtoFuelCurve, PartLoadRat);
            EngineDrivenFuelEnergy = modQEvaporator / ClngLoadFuelRat;
        }
        // Use Curve fit to determine energy recovered in the water jacket.  This curve calculates the water jacket energy recovered (J/s) by
        // multiplying the total fuel input (J/s) by the fraction of that power that could be recovered in the water jacket at that
        // particular part load.

        RecJacHeattoFuelRat = CurveManager::CurveValue(EngineDrivenChiller(ChillerNum).RecJacHeattoFuelCurve, PartLoadRat);
        EngineDrivenChiller(ChillerNum).modQJacketRecovered = EngineDrivenFuelEnergy * RecJacHeattoFuelRat;

        // Use Curve fit to determine Heat Recovered Lubricant Energy.  This curve calculates the lube energy recovered (J/s) by
        // multiplying the total fuel input (J/s) by the fraction of that power that could be recovered in the lube oil at that
        // particular part load.
        RecLubeHeattoFuelRat = CurveManager::CurveValue(EngineDrivenChiller(ChillerNum).RecLubeHeattoFuelCurve, PartLoadRat);
        EngineDrivenChiller(ChillerNum).modQLubeOilRecovered = EngineDrivenFuelEnergy * RecLubeHeattoFuelRat;

        // Use Curve fit to determine Heat Recovered from the exhaust.  This curve calculates the  energy recovered (J/s) by
        // multiplying the total fuel input (J/s) by the fraction of that power that could be recovered in the exhaust at that
        // particular part load.
        TotExhausttoFuelRat = CurveManager::CurveValue(EngineDrivenChiller(ChillerNum).TotExhausttoFuelCurve, PartLoadRat);
        TotalExhaustEnergy = EngineDrivenFuelEnergy * TotExhausttoFuelRat;

        // Use Curve fit to determine Exhaust Temperature in C.  The temperature is simply a curve fit
        // of the exhaust temperature in C to the part load ratio.
        if (PartLoadRat != 0) {
            ExhaustTemp = CurveManager::CurveValue(EngineDrivenChiller(ChillerNum).ExhaustTempCurve, PartLoadRat);
            ExhaustGasFlow = TotalExhaustEnergy / (ExhaustCP * (ExhaustTemp - ReferenceTemp));

            // Use Curve fit to determine stack temp after heat recovery
            UA = EngineDrivenChiller(ChillerNum).UACoef(1) * std::pow(ChillerNomCap, EngineDrivenChiller(ChillerNum).UACoef(2));

            DesignMinExitGasTemp = EngineDrivenChiller(ChillerNum).DesignMinExitGasTemp;
            EngineDrivenChiller(ChillerNum).modExhaustStackTemp = DesignMinExitGasTemp + (ExhaustTemp - DesignMinExitGasTemp) /
                                                          std::exp(UA / (max(ExhaustGasFlow, MaxExhaustperPowerOutput * ChillerNomCap) * ExhaustCP));

            EngineDrivenChiller(ChillerNum).modQExhaustRecovered = max(ExhaustGasFlow * ExhaustCP * (ExhaustTemp - EngineDrivenChiller(ChillerNum).modExhaustStackTemp), 0.0);
        } else {
            EngineDrivenChiller(ChillerNum).modQExhaustRecovered = 0.0;
        }

        EngineDrivenChiller(ChillerNum).modQTotalHeatRecovered = EngineDrivenChiller(ChillerNum).modQExhaustRecovered + EngineDrivenChiller(ChillerNum).modQLubeOilRecovered + EngineDrivenChiller(ChillerNum).modQJacketRecovered;

        // Update Heat Recovery temperatures
        if (EngineDrivenChiller(ChillerNum).HeatRecActive) {
            CalcEngineChillerHeatRec(ChillerNum, EngineDrivenChiller(ChillerNum).modQTotalHeatRecovered, HeatRecRatio);
            EngineDrivenChiller(ChillerNum).modQExhaustRecovered *= HeatRecRatio;
            EngineDrivenChiller(ChillerNum).modQLubeOilRecovered *= HeatRecRatio;
            EngineDrivenChiller(ChillerNum).modQJacketRecovered *= HeatRecRatio;
        }

        // Calculate Energy
        modCondenserEnergy = modQCondenser * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEnergy = modPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEvaporatorEnergy = modQEvaporator * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        EngineDrivenChiller(ChillerNum).modFuelEnergyUseRate = EngineDrivenFuelEnergy;
        EngineDrivenChiller(ChillerNum).modFuelEnergy = EngineDrivenChiller(ChillerNum).modFuelEnergyUseRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        EngineDrivenChiller(ChillerNum).modJacketEnergyRec = EngineDrivenChiller(ChillerNum).modQJacketRecovered * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        EngineDrivenChiller(ChillerNum).modLubeOilEnergyRec = EngineDrivenChiller(ChillerNum).modQLubeOilRecovered * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        EngineDrivenChiller(ChillerNum).modExhaustEnergyRec = EngineDrivenChiller(ChillerNum).modQExhaustRecovered * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        EngineDrivenChiller(ChillerNum).modQTotalHeatRecovered = EngineDrivenChiller(ChillerNum).modQExhaustRecovered + EngineDrivenChiller(ChillerNum).modQLubeOilRecovered + EngineDrivenChiller(ChillerNum).modQJacketRecovered;
        EngineDrivenChiller(ChillerNum).modTotalHeatEnergyRec = EngineDrivenChiller(ChillerNum).modExhaustEnergyRec + EngineDrivenChiller(ChillerNum).modLubeOilEnergyRec + EngineDrivenChiller(ChillerNum).modJacketEnergyRec;
        EngineDrivenChiller(ChillerNum).modFuelEnergyUseRate = std::abs(EngineDrivenChiller(ChillerNum).modFuelEnergyUseRate);
        EngineDrivenChiller(ChillerNum).modFuelEnergy = std::abs(EngineDrivenChiller(ChillerNum).modFuelEnergy);
        EngineDrivenChiller(ChillerNum).modFuelMdot = std::abs(EngineDrivenChiller(ChillerNum).modFuelEnergyUseRate) / (EngineDrivenChiller(ChillerNum).FuelHeatingValue * KJtoJ);

        // check for problems BG 9/12/06 (deal with observed negative energy results)
        if (modEnergy < 0.0) { // there is a serious problem
            if (EngineDrivenChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                // first check for run away condenser loop temps (only reason yet to be observed for this?)
                if (_CondInletTemp > 70.0) {
                    ShowSevereError("CalcEngineDrivenChillerModel: Condenser loop inlet temperatures > 70.0 C for EngineDrivenChiller=" +
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Condenser loop water temperatures are too high at" + RoundSigDigits(_CondInletTemp, 2));
                    ShowContinueError("Check input for condenser plant loop, especially cooling tower");
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));

                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            if (!WarmupFlag) {
                if (AvailNomCapRat < 0.0) { // apparently the real reason energy goes negative
                    ShowSevereError("CalcEngineDrivenChillerModel: Capacity ratio below zero for EngineDrivenChiller=" +
                                    EngineDrivenChiller(ChillerNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Check input for Capacity Ratio Curve");
                    ShowContinueError("Condenser inlet temperature: " + RoundSigDigits(_CondInletTemp, 2));
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));
                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            // If makes it here, set limits, chiller can't have negative energy/power
            // proceeding silently for now but may want to throw error here
            modPower = 0.0;
            modEnergy = 0.0;
        }
    }

    void CalcGTChillerModel(int &ChillerNum,        // chiller number
                            Real64 &MyLoad,         // operating load
                            bool const RunFlag,     // TRUE when chiller operating
                            int const EquipFlowCtrl // Flow control mode for the equipment
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Dan Fisher / Brandon Anderson
        //       DATE WRITTEN   Sept. 2000
        //       MODIFIED       Feb. 2010, Chandan Sharma, FSEC. Added basin heater
        //                      Jun. 2016, Rongpeng Zhang, LBNL. Applied the chiller supply water temperature sensor fault model
        //                      Nov. 2016, Rongpeng Zhang, LBNL. Added Fouling Chiller fault
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // simulate a vapor compression chiller using the GT model

        // METHODOLOGY EMPLOYED:
        // curve fit of performance data:

        // REFERENCES:
        // 1. BLAST Users Manual
        // 2. CHILLER User Manual

        // Locals
        Real64 _ExhaustStackTemp(0.0); // Temperature of Exhaust Gases
        Real64 _EvapInletTemp;         // C - evaporator inlet temperature, water side
        Real64 _CondInletTemp;         // C - condenser inlet temperature, water side

        Real64 const ExhaustCP(1.047); // Exhaust Gas Specific Heat
        static ObjexxFCL::gio::Fmt OutputFormat("(F6.2)");
        static std::string const RoutineName("CalcGTChillerModel");
        static std::string const RoutineNameHeatRecovery("ChillerHeatRecovery");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 MinPartLoadRat;                  // min allowed operating frac full load
        Real64 MaxPartLoadRat;                  // max allowed operating frac full load
        Real64 TempCondIn;                      // C - (GT ADJTC(1)The design secondary loop fluid
        Real64 TempCondInDesign;                // C - (GT ADJTC(1)The design secondary loop fluid
        Real64 TempRiseRat;                     // intermediate result:  temperature rise ratio
        Real64 TempEvapOut;                     // C - evaporator outlet temperature, water side
        Real64 TempEvapOutSetPoint(0.0);        // C - evaporator outlet temperature setpoint
        Real64 TempEvapOutDesign;               // design evaporator outlet temperature, water side
        Real64 ChillerNomCap;                   // chiller nominal capacity
        Real64 AvailChillerCap;                 // chiller available capacity
        Real64 COP;                             // coefficient of performance
        Real64 FracFullLoadPower;               // fraction of full load power
        Real64 EvapDeltaTemp(0.0);              // C - evaporator temperature difference, water side
        Real64 DeltaTemp;                       // C - intermediate result: condenser/evaporator temp diff
        Real64 AvailNomCapRat;                  // intermediate result: available nominal capacity ratio
        Real64 FullLoadPowerRat;                // intermediate result: full load power ratio
        Real64 PartLoadRat(0.0);                // part load ratio for efficiency calculations
        Real64 OperPartLoadRat;                 // Actual Operating PLR
        int EvapInletNode;                      // evaporator inlet node number, water side
        int EvapOutletNode;                     // evaporator outlet node number, water side
        int CondInletNode;                      // condenser inlet node number, water side
        int CondOutletNode;                     // condenser outlet node number, water side
        Real64 EvapMassFlowRateMax(0.0); // Max Design Evaporator Mass Flow Rate converted from Volume Flow Rate
        Real64 TempLowLimitEout;                // C - Evaporator low temp. limit cut off
        // Special variables for GT Chiller
        Real64 RPLoad;
        Real64 PLoad;
        Real64 GTEngineCapacity;     // Capacity of GT Unit attached to Chiller
        Real64 MaxExhaustperGTPower; // Maximum Exhaust Flow per KW Power Out
        Real64 RL;
        Real64 RL2;

        Real64 FuelEnergyIn(0.0);              // (EFUEL) Amount of Fuel Energy Required to run gas turbine
        Real64 ExhaustFlow(0.0);               // (FEX) Exhaust Gas Flow Rate cubic meters per second
        Real64 ExhaustTemp(0.0);               // (TEX) Exhaust Gas Temperature in C
        Real64 QHeatRecLube;                   // (ELUBE) Recoverable Lube Oil Energy (W)
        Real64 UAtoCapRat;                     // (UACGC) Heat Exchanger UA to Capacity
        Real64 AmbientDeltaT;                  // (ATAIR) Difference between ambient actual and ambient design temperatures
        Real64 DesignSteamSatTemp;             // Saturization Temperature of Steam in Stack
        Real64 CurrentEndTime;                 // end time of time step for current simulation time step
        std::string OutputChar;         // character string for warning messages

        int HeatRecInNode;          // Heat Recovery Fluid Inlet Node Num
        Real64 HeatRecInTemp(0.0);  // Heat Recovery Fluid Inlet Temperature
        Real64 HeatRecOutTemp(0.0); // Heat Recovery Fluid Outlet Temperature
        Real64 HeatRecMdot(0.0);    // Heat Recovery Fluid Mass FlowRate
        Real64 HeatRecCp;           // Specific Heat of the Heat Recovery Fluid
        Real64 FuelHeatingValue;    // Heating Value of Fuel in kJ/kg
        Real64 MinHeatRecMdot(0.0); // Mass Flow rate that keeps from exceeding max temp
        Real64 HeatRecRatio;        // Reduced ratio to multiply recovered heat terms by
        Real64 FRAC;

        int LoopNum;
        int LoopSideNum;
        Real64 Cp;     // local for fluid specif heat, for evaporator
        Real64 CpCond; // local for fluid specif heat, for condenser

        // set module level inlet and outlet nodes
        modEvapMassFlowRate = 0.0;
        modCondMassFlowRate = 0.0;
        modPower = 0.0;
        modQCondenser = 0.0;
        modQEvaporator = 0.0;
        modEnergy = 0.0;
        modCondenserEnergy = 0.0;
        modEvaporatorEnergy = 0.0;
        EvapInletNode = GTChiller(ChillerNum).Base.EvapInletNodeNum;
        EvapOutletNode = GTChiller(ChillerNum).Base.EvapOutletNodeNum;
        CondInletNode = GTChiller(ChillerNum).Base.CondInletNodeNum;
        CondOutletNode = GTChiller(ChillerNum).Base.CondOutletNodeNum;
        HeatRecInNode = GTChiller(ChillerNum).HeatRecInletNodeNum;
        QHeatRecLube = 0.0;
        FRAC = 1.0;
        LoopNum = GTChiller(ChillerNum).Base.CWLoopNum;
        LoopSideNum = GTChiller(ChillerNum).Base.CWLoopSideNum;
        _EvapInletTemp = Node(EvapInletNode).Temp;

        // calculate end time of current time step
        CurrentEndTime = DataGlobals::CurrentTime + DataHVACGlobals::SysTimeElapsed;

        // Print warning messages only when valid and only for the first ocurrance. Let summary provide statistics.
        // Wait for next time step to print warnings. If simulation iterates, print out
        // the warning for the last iteration only. Must wait for next time step to accomplish this.
        // If a warning occurs and the simulation down shifts, the warning is not valid.
        if (CurrentEndTime > GTChiller(ChillerNum).Base.CurrentEndTimeLast && DataHVACGlobals::TimeStepSys >= GTChiller(ChillerNum).Base.TimeStepSysLast) {
            if (GTChiller(ChillerNum).Base.PrintMessage) {
                ++GTChiller(ChillerNum).Base.MsgErrorCount;
                // Show single warning and pass additional info to ShowRecurringWarningErrorAtEnd
                if (GTChiller(ChillerNum).Base.MsgErrorCount < 2) {
                    ShowWarningError(GTChiller(ChillerNum).Base.MsgBuffer1 + '.');
                    ShowContinueError(GTChiller(ChillerNum).Base.MsgBuffer2);
                } else {
                    ShowRecurringWarningErrorAtEnd(GTChiller(ChillerNum).Base.MsgBuffer1 + " error continues.",
                                                   GTChiller(ChillerNum).Base.ErrCount1,
                                                   GTChiller(ChillerNum).Base.MsgDataLast,
                                                   GTChiller(ChillerNum).Base.MsgDataLast,
                                                   _,
                                                   "[C]",
                                                   "[C]");
                }
            }
        }

        // save last system time step and last end time of current time step (used to determine if warning is valid)
        GTChiller(ChillerNum).Base.TimeStepSysLast = DataHVACGlobals::TimeStepSys;
        GTChiller(ChillerNum).Base.CurrentEndTimeLast = CurrentEndTime;

        // If Chiller load is 0 or chiller is not running then leave the subroutine.Before leaving
        // if the component control is SERIESACTIVE we set the component flow to inlet flow so that
        // flow resolver will not shut down the branch
        if (MyLoad >= 0.0 || !RunFlag) {
            if (EquipFlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive || DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 1) {
                modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            } else {
                modEvapMassFlowRate = 0.0;

                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     GTChiller(ChillerNum).Base.CWLoopNum,
                                     GTChiller(ChillerNum).Base.CWLoopSideNum,
                                     GTChiller(ChillerNum).Base.CWBranchNum,
                                     GTChiller(ChillerNum).Base.CWCompNum);
            }
            if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                if (DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CDLoopNum)
                        .LoopSide(GTChiller(ChillerNum).Base.CDLoopSideNum)
                        .Branch(GTChiller(ChillerNum).Base.CDBranchNum)
                        .Comp(GTChiller(ChillerNum).Base.CDCompNum)
                        .FlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive) {
                    modCondMassFlowRate = Node(CondInletNode).MassFlowRate;
                } else {
                    modCondMassFlowRate = 0.0;
                    PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                         CondInletNode,
                                         CondOutletNode,
                                         GTChiller(ChillerNum).Base.CDLoopNum,
                                         GTChiller(ChillerNum).Base.CDLoopSideNum,
                                         GTChiller(ChillerNum).Base.CDBranchNum,
                                         GTChiller(ChillerNum).Base.CDCompNum);
                }
            }

            if (GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(GTChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff,
                                     GTChiller(ChillerNum).Base.BasinHeaterSchedulePtr,
                                     GTChiller(ChillerNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }
            GTChiller(ChillerNum).Base.PrintMessage = false;
            return;
        }

        if (GTChiller(ChillerNum).Base.CondenserType == AirCooled) { // Condenser inlet temp = outdoor temp
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirDryBulb;
            //  Warn user if entering condenser temperature falls below 0C
            if (Node(CondInletNode).Temp < 0.0 && !WarmupFlag) {
                GTChiller(ChillerNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                GTChiller(ChillerNum).Base.MsgBuffer1 = "CalcGasTurbineChillerModel - Chiller:CombustionTurbine \"" +
                                                        GTChiller(ChillerNum).Base.Name + "\" - Air Cooled Condenser Inlet Temperature below 0C";
                GTChiller(ChillerNum).Base.MsgBuffer2 = "... Outdoor Dry-bulb Condition = " + OutputChar +
                                                        " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                    General::CreateSysTimeIntervalString();
                GTChiller(ChillerNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                GTChiller(ChillerNum).Base.PrintMessage = false;
            }
        } else if (GTChiller(ChillerNum).Base.CondenserType == EvapCooled) { // Condenser inlet temp = (outdoor wet bulb)
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirWetBulb;
            //  Warn user if evap condenser wet bulb temperature falls below 10C
            if (Node(CondInletNode).Temp < 10.0 && !WarmupFlag) {
                GTChiller(ChillerNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                GTChiller(ChillerNum).Base.MsgBuffer1 = "CalcGasTurbineChillerModel - Chiller:CombustionTurbine \"" +
                                                        GTChiller(ChillerNum).Base.Name + "\" - Evap Cooled Condenser Inlet Temperature below 10C";
                GTChiller(ChillerNum).Base.MsgBuffer2 = "... Outdoor Wet-bulb Condition = " + OutputChar +
                                                        " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                    General::CreateSysTimeIntervalString();
                GTChiller(ChillerNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                GTChiller(ChillerNum).Base.PrintMessage = false;
            }
        } // End of the Air Cooled/Evap Cooled Logic block

        // If not air or evap cooled then set to the condenser node that is attached to a cooling tower
        _CondInletTemp = Node(CondInletNode).Temp;

        // Set mass flow rates
        if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {
            modCondMassFlowRate = GTChiller(ChillerNum).Base.CondMassFlowRateMax;
            PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                 CondInletNode,
                                 CondOutletNode,
                                 GTChiller(ChillerNum).Base.CDLoopNum,
                                 GTChiller(ChillerNum).Base.CDLoopSideNum,
                                 GTChiller(ChillerNum).Base.CDBranchNum,
                                 GTChiller(ChillerNum).Base.CDCompNum);
            PlantUtilities::PullCompInterconnectTrigger(GTChiller(ChillerNum).Base.CWLoopNum,
                                        GTChiller(ChillerNum).Base.CWLoopSideNum,
                                        GTChiller(ChillerNum).Base.CWBranchNum,
                                        GTChiller(ChillerNum).Base.CWCompNum,
                                        GTChiller(ChillerNum).Base.CondMassFlowIndex,
                                        GTChiller(ChillerNum).Base.CDLoopNum,
                                        GTChiller(ChillerNum).Base.CDLoopSideNum,
                                        DataPlant::CriteriaType_MassFlowRate,
                                                        modCondMassFlowRate);

            if (modCondMassFlowRate < DataBranchAirLoopPlant::MassFlowTolerance) return;
        }

        //  LOAD LOCAL VARIABLES FROM DATA STRUCTURE (for code readability)
        auto const &CapacityRat(GTChiller(ChillerNum).CapRatCoef);
        auto const &PowerRat(GTChiller(ChillerNum).PowerRatCoef);
        auto const &FullLoadFactor(GTChiller(ChillerNum).FullLoadCoef);
        MinPartLoadRat = GTChiller(ChillerNum).MinPartLoadRat;
        MaxPartLoadRat = GTChiller(ChillerNum).MaxPartLoadRat;
        TempCondInDesign = GTChiller(ChillerNum).TempDesCondIn;
        TempRiseRat = GTChiller(ChillerNum).TempRiseCoef;
        TempEvapOutDesign = GTChiller(ChillerNum).TempDesEvapOut;
        ChillerNomCap = GTChiller(ChillerNum).Base.NomCap;
        COP = GTChiller(ChillerNum).Base.COP;
        TempCondIn = Node(GTChiller(ChillerNum).Base.CondInletNodeNum).Temp;
        TempEvapOut = Node(GTChiller(ChillerNum).Base.EvapOutletNodeNum).Temp;
        TempLowLimitEout = GTChiller(ChillerNum).TempLowLimitEvapOut;
        EvapMassFlowRateMax = GTChiller(ChillerNum).Base.EvapMassFlowRateMax;
        LoopNum = GTChiller(ChillerNum).Base.CWLoopNum;
        LoopSideNum = GTChiller(ChillerNum).Base.CWLoopSideNum;

        // If there is a fault of chiller fouling (zrp_Nov2016)
        if (GTChiller(ChillerNum).Base.FaultyChillerFoulingFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = GTChiller(ChillerNum).Base.FaultyChillerFoulingIndex;
            Real64 NomCap_ff = ChillerNomCap;
            Real64 COP_ff = COP;

            // calculate the Faulty Chiller Fouling Factor using fault information
            GTChiller(ChillerNum).Base.FaultyChillerFoulingFactor = FaultsManager::FaultsChillerFouling(FaultIndex).CalFoulingFactor();

            // update the Chiller nominal capacity and COP at faulty cases
            ChillerNomCap = NomCap_ff * GTChiller(ChillerNum).Base.FaultyChillerFoulingFactor;
            COP = COP_ff * GTChiller(ChillerNum).Base.FaultyChillerFoulingFactor;
        }

        // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
        if (GTChiller(ChillerNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = GTChiller(ChillerNum).Base.FaultyChillerSWTIndex;
            Real64 EvapOutletTemp_ff = TempEvapOut;

            // calculate the sensor offset using fault information
            GTChiller(ChillerNum).Base.FaultyChillerSWTOffset = FaultsManager::FaultsChillerSWTSensor(FaultIndex).CalFaultOffsetAct();
            // update the TempEvapOut
            TempEvapOut = max(GTChiller(ChillerNum).TempLowLimitEvapOut,
                              min(Node(EvapInletNode).Temp, EvapOutletTemp_ff - GTChiller(ChillerNum).Base.FaultyChillerSWTOffset));
            GTChiller(ChillerNum).Base.FaultyChillerSWTOffset = EvapOutletTemp_ff - TempEvapOut;
        }

        // Calculate chiller performance from this set of performance equations.
        //  from BLAST...Z=(TECONDW-ADJTC(1))/ADJTC(2)-(TLCHLRW-ADJTC(3))
        DeltaTemp = (TempCondIn - TempCondInDesign) / TempRiseRat - (TempEvapOut - TempEvapOutDesign);

        //  from BLAST...RCAV=RCAVC(1)+RCAVC(2)*Z+RCAVC(3)*Z**2
        AvailNomCapRat = CapacityRat(1) + CapacityRat(2) * DeltaTemp + CapacityRat(3) * pow_2(DeltaTemp);

        AvailChillerCap = ChillerNomCap * AvailNomCapRat;

        // from BLAST...G=ADJEC(1)+ADJEC(2)*RCAV+ADJEC(3)*RCAV**2.
        FullLoadPowerRat = PowerRat(1) + PowerRat(2) * AvailNomCapRat + PowerRat(3) * pow_2(AvailNomCapRat);

        //  from BLAST...RCLOAD=AMAX1(MINCHFR(I,IPLCTR),AMIN1(CHLRLOAD(I)/CHLROCAP(I) &
        //         /RCAV,MAXCHFR(I,IPLCTR)))
        if (AvailChillerCap > 0.0) {
            PartLoadRat = max(MinPartLoadRat, min(std::abs(MyLoad) / AvailChillerCap, MaxPartLoadRat));
        }

        // from BLAST...RPOWER=RPWRC(1)+RPWRC(2)*RCLOAD+RPWRC(3)*RCLOAD**2
        FracFullLoadPower = FullLoadFactor(1) + FullLoadFactor(2) * PartLoadRat + FullLoadFactor(3) * pow_2(PartLoadRat);

        if (AvailChillerCap > 0.0) {
            if (std::abs(MyLoad) / AvailChillerCap < MinPartLoadRat) {
                OperPartLoadRat = std::abs(MyLoad) / AvailChillerCap;
            } else {
                OperPartLoadRat = PartLoadRat;
            }
        } else {
            OperPartLoadRat = 0.0;
        }

        Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CWLoopNum).FluidName,
                                   Node(EvapInletNode).Temp,
                                   DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);
        // If FlowLock is True, the new resolved mdot is used to update Power, QEvap, Qcond, and
        // condenser side outlet temperature.
        if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0) {
            GTChiller(ChillerNum).Base.PossibleSubcooling = false;
            modQEvaporator = AvailChillerCap * OperPartLoadRat;
            if (OperPartLoadRat < MinPartLoadRat) {
                FRAC = min(1.0, (OperPartLoadRat / MinPartLoadRat));
            } else {
                FRAC = 1.0;
            }
            modPower = FracFullLoadPower * FullLoadPowerRat * AvailChillerCap / COP * FRAC;

            // Either set the flow to the Constant value or caluclate the flow for the variable volume
            if ((GTChiller(ChillerNum).Base.FlowMode == ConstantFlow) || (GTChiller(ChillerNum).Base.FlowMode == NotModulated)) {
                // Start by assuming max (design) flow
                modEvapMassFlowRate = EvapMassFlowRateMax;
                // Use SetComponentFlowRate to decide actual flow
                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     GTChiller(ChillerNum).Base.CWLoopNum,
                                     GTChiller(ChillerNum).Base.CWLoopSideNum,
                                     GTChiller(ChillerNum).Base.CWBranchNum,
                                     GTChiller(ChillerNum).Base.CWCompNum);
                // Evaluate delta temp based on actual flow rate
                if (modEvapMassFlowRate != 0.0) {
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                } else {
                    EvapDeltaTemp = 0.0;
                }
                // Evaluate outlet temp based on delta
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
            } else if (GTChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated) {
                // Calculate the Delta Temp from the inlet temp to the chiller outlet setpoint
                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CWLoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        EvapDeltaTemp = Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPoint;
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        EvapDeltaTemp = Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPointHi;
                    }
                }
                if (EvapDeltaTemp != 0.0) {
                    // Calculate desired flow to request based on load
                    modEvapMassFlowRate = std::abs(modQEvaporator / Cp / EvapDeltaTemp);
                    if ((modEvapMassFlowRate - EvapMassFlowRateMax) > DataBranchAirLoopPlant::MassFlowTolerance) GTChiller(ChillerNum).Base.PossibleSubcooling = true;
                    // Check to see if the Maximum is exceeded, if so set to maximum
                    modEvapMassFlowRate = min(EvapMassFlowRateMax, modEvapMassFlowRate);
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         GTChiller(ChillerNum).Base.CWLoopNum,
                                         GTChiller(ChillerNum).Base.CWLoopSideNum,
                                         GTChiller(ChillerNum).Base.CWBranchNum,
                                         GTChiller(ChillerNum).Base.CWCompNum);
                    {
                        auto const SELECT_CASE_var(DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CWLoopNum).LoopDemandCalcScheme);
                        if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPoint;
                        } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPointHi;
                        }
                    }
                } else {
                    // Try to request zero flow
                    modEvapMassFlowRate = 0.0;
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         GTChiller(ChillerNum).Base.CWLoopNum,
                                         GTChiller(ChillerNum).Base.CWLoopSideNum,
                                         GTChiller(ChillerNum).Base.CWBranchNum,
                                         GTChiller(ChillerNum).Base.CWCompNum);
                    // No deltaT since component is not running
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            } // End of Constant Variable Flow If Block

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (GTChiller(ChillerNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = GTChiller(ChillerNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = (GTChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated);
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        GTChiller(ChillerNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                PartLoadRat = (AvailChillerCap > 0.0) ? (modQEvaporator / AvailChillerCap) : 0.0;
                PartLoadRat = max(0.0, min(PartLoadRat, MaxPartLoadRat));
                // ChillerPartLoadRatio = PartLoadRat;
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

        } else { // If FlowLock is True

            modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                 EvapInletNode,
                                 EvapOutletNode,
                                 GTChiller(ChillerNum).Base.CWLoopNum,
                                 GTChiller(ChillerNum).Base.CWLoopSideNum,
                                 GTChiller(ChillerNum).Base.CWBranchNum,
                                 GTChiller(ChillerNum).Base.CWCompNum);
            //       Some other component set the flow to 0. No reason to continue with calculations.
            if (modEvapMassFlowRate == 0.0) {
                MyLoad = 0.0;
                if (GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                    CalcBasinHeaterPower(GTChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff,
                                         GTChiller(ChillerNum).Base.BasinHeaterSchedulePtr,
                                         GTChiller(ChillerNum).Base.BasinHeaterSetPointTemp,
                                         modBasinHeaterPower);
                }
                GTChiller(ChillerNum).Base.PrintMessage = false;
                return;
            }

            if (GTChiller(ChillerNum).Base.PossibleSubcooling) {
                modQEvaporator = std::abs(MyLoad);
                EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
            } else { // No subcooling in this case.No recalculation required.Still need to check chiller low temp limit
                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        if ((GTChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated) ||
                            (DataPlant::PlantLoop(LoopNum)
                                 .LoopSide(LoopSideNum)
                                 .Branch(GTChiller(ChillerNum).Base.CWBranchNum)
                                 .Comp(GTChiller(ChillerNum).Base.CWCompNum)
                                 .CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                            (Node(EvapOutletNode).TempSetPoint != SensedNodeFlagValue)) {
                            TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPoint;
                        } else {
                            TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPoint;
                        }
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        if ((GTChiller(ChillerNum).Base.FlowMode == LeavingSetPointModulated) ||
                            (DataPlant::PlantLoop(LoopNum)
                                 .LoopSide(LoopSideNum)
                                 .Branch(GTChiller(ChillerNum).Base.CWBranchNum)
                                 .Comp(GTChiller(ChillerNum).Base.CWCompNum)
                                 .CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                            (Node(EvapOutletNode).TempSetPointHi != SensedNodeFlagValue)) {
                            TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPointHi;
                        } else {
                            TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPointHi;
                        }
                    }
                }
                EvapDeltaTemp = Node(EvapInletNode).Temp - TempEvapOutSetPoint;
                modQEvaporator = std::abs(modEvapMassFlowRate * Cp * EvapDeltaTemp);
                modEvapOutletTemp = TempEvapOutSetPoint;
            }
            // Check that the Evap outlet temp honors both plant loop temp low limit and also the chiller low limit
            if (modEvapOutletTemp < TempLowLimitEout) {
                if ((Node(EvapInletNode).Temp - TempLowLimitEout) > DeltaTempTol) {
                    modEvapOutletTemp = TempLowLimitEout;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }
            if (modEvapOutletTemp < Node(EvapOutletNode).TempMin) {
                if ((Node(EvapInletNode).Temp - Node(EvapOutletNode).TempMin) > DeltaTempTol) {
                    modEvapOutletTemp = Node(EvapOutletNode).TempMin;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }
            // If load exceeds the distributed load set to the distributed load
            if (modQEvaporator > std::abs(MyLoad)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = std::abs(MyLoad);
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (GTChiller(ChillerNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = GTChiller(ChillerNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = false;
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        GTChiller(ChillerNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

            // Checks QEvaporator on the basis of the machine limits.
            if (modQEvaporator > (AvailChillerCap * MaxPartLoadRat)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = AvailChillerCap * PartLoadRat;
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            if (OperPartLoadRat < MinPartLoadRat) {
                FRAC = min(1.0, (OperPartLoadRat / MinPartLoadRat));
            } else {
                FRAC = 1.0;
            }

            // set the module level variable used for reporting FRAC
            modChillerCyclingRatio = FRAC;

            // Chiller is false loading below PLR = minimum unloading ratio, find PLR used for energy calculation
            modPower = FracFullLoadPower * FullLoadPowerRat * AvailChillerCap / COP * FRAC;

            if (modEvapMassFlowRate == 0.0) {
                modQEvaporator = 0.0;
                modEvapOutletTemp = Node(EvapInletNode).Temp;
                modPower = 0.0;
                GTChiller(ChillerNum).Base.PrintMessage = false;
            }
            if (modQEvaporator == 0.0 && GTChiller(ChillerNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(GTChiller(ChillerNum).Base.BasinHeaterPowerFTempDiff,
                                     GTChiller(ChillerNum).Base.BasinHeaterSchedulePtr,
                                     GTChiller(ChillerNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }

        } // This is the end of the FlowLock Block

        // Now determine Cooling
        // QCondenser is calculated the same for each type, but the power consumption should be different
        //  depending on the performance coefficients used for the chiller model.
        modQCondenser = modPower + modQEvaporator;

        if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {

            if (modCondMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                CpCond = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CDLoopNum).FluidName,
                                                                _CondInletTemp,
                                               DataPlant::PlantLoop(GTChiller(ChillerNum).Base.CDLoopNum).FluidIndex,
                                               RoutineName);
                modCondOutletTemp = modQCondenser / modCondMassFlowRate / CpCond + _CondInletTemp;
            } else {
                ShowSevereError("CalcGasTurbineChillerModel: Condenser flow = 0, for GasTurbineChiller=" + GTChiller(ChillerNum).Base.Name);
                ShowContinueErrorTimeStamp("");
            }

        } else { // Air Cooled or Evap Cooled

            // don't care about outlet temp for Air-Cooled or Evap Cooled and there is no CondMassFlowRate and would divide by zero
            modCondOutletTemp = _CondInletTemp;
        }

        // Special GT Chiller Variables
        // Gas Turbine Driven Portion of the Chiller:

        GTEngineCapacity = GTChiller(ChillerNum).GTEngineCapacity;
        MaxExhaustperGTPower = GTChiller(ChillerNum).MaxExhaustperGTPower;

        // Note: All Old Blast Code comments begin at left.

        // D                                   COMPUTE TOWER CLOAD
        //               ETOWER(TypeIndex) = PREQD + CHLRLOAD(TypeIndex)
        //               RPLOAD = PREQD/CHLROCAP(TypeIndex)
        //               IF (RFLAGS(81)) WRITE (OUTPUT,703) PREQD,ETOWER(TypeIndex),RPLOAD
        //               IF (PREQD .GT. 0.0d0) THEN
        if (AvailChillerCap > 0) {
            RPLoad = modPower / AvailChillerCap;
        } else {
            RPLoad = 0.0;
        }

        if (modPower > 0) {
            // D$                               FOR EACH CHILLER OPERATING
            //                  MAXSZ = NUMCHSIZ(TypeIndex,IPLCTR)
            //                  DO IS = 1,MAXSZ
            //                     NUMOPR = CHLRIOPR(IS,TypeIndex)
            //                     IF (NUMOPR.GT.0) THEN
            //                        PLOAD = CHNOMCAP(IS,TypeIndex,IPLCTR) * RPLOAD

            PLoad = ChillerNomCap * RPLoad;

            // D$                                COMPUTE FUEL AND WASTE HEAT
            //     TEX IS CALCULATED USING COEFFICIENTS TEX2GC( ) TO RESULT IN TEMP.
            //     DEGREES ACTUAL, HENCE THE NECESSARY CONVERSION ?-273.?
            //                        RLOAD=AMAX1(PLOAD/CHLROCAP(TypeIndex),MINCHFR(TypeIndex,IPLCTR))
            //                        RLD2 = RLOAD**2

            // RL = MAX(PLoad/GTEngineCapacity, MinPartLoadRat * ChillerNomCap)
            RL = max(PLoad / ChillerNomCap, MinPartLoadRat);
            RL2 = pow_2(RL);

            //     ATAIR = DELTA TEMPERATURE. ACTUAL - 25 DEG.C (77 DEG.F)
            //                                RATING POINT
            //                        ATAIR = ODB - 25.
            //                        TAR2=ATAIR**2

            // ??? Not sure about this Ambient Actual Temp - also do we need to have design ambient as input?

            if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                AmbientDeltaT = DataEnvironment::OutDryBulbTemp - 25.0;
            } else { // air or evap cooled
                AmbientDeltaT = Node(CondInletNode).OutAirDryBulb - 25.0;
            }

            //                        EFUEL=PLOAD*(FUL1GC(1,IPLCTR)+FUL1GC(2,IPLCTR)*  &
            //                              RLOAD+FUL1GC(3,IPLCTR)*RLD2)*              &
            //                              (FUL2GC(1,IPLCTR)+FUL2GC(2,IPLCTR)*ATAIR+  &
            //                              FUL2GC(3,IPLCTR)*TAR2)

            FuelEnergyIn = PLoad *
                           (GTChiller(ChillerNum).PLBasedFuelInputCoef(1) + GTChiller(ChillerNum).PLBasedFuelInputCoef(2) * RL +
                            GTChiller(ChillerNum).PLBasedFuelInputCoef(3) * RL2) *
                           (GTChiller(ChillerNum).TempBasedFuelInputCoef(1) + GTChiller(ChillerNum).TempBasedFuelInputCoef(2) * AmbientDeltaT +
                            GTChiller(ChillerNum).TempBasedFuelInputCoef(3) * pow_2(AmbientDeltaT));

            //                        FEX=GTDSLCAP(IS,TypeIndex,IPLCTR)*(FEXGC(1,IPLCTR)+      &
            //                            FEXGC(2,IPLCTR)*ATAIR+FEXGC(3,IPLCTR)*TAR2)

            ExhaustFlow = GTEngineCapacity * (GTChiller(ChillerNum).ExhaustFlowCoef(1) + GTChiller(ChillerNum).ExhaustFlowCoef(2) * AmbientDeltaT +
                                              GTChiller(ChillerNum).ExhaustFlowCoef(3) * pow_2(AmbientDeltaT));

            //                        TEX=(TEX1GC(1,IPLCTR)+TEX1GC(2,IPLCTR)*RLOAD+    &
            //                            TEX1GC(3,IPLCTR)*RLD2)*(TEX2GC(1,IPLCTR)+    &
            //                            TEX2GC(2,IPLCTR)*ATAIR+TEX2GC(3,IPLCTR)*     &
            //                            TAR2)-273.

            ExhaustTemp = (GTChiller(ChillerNum).PLBasedExhaustTempCoef(1) + GTChiller(ChillerNum).PLBasedExhaustTempCoef(2) * RL +
                           GTChiller(ChillerNum).PLBasedExhaustTempCoef(3) * RL2) *
                              (GTChiller(ChillerNum).TempBasedExhaustTempCoef(1) + GTChiller(ChillerNum).TempBasedExhaustTempCoef(2) * AmbientDeltaT +
                               GTChiller(ChillerNum).TempBasedExhaustTempCoef(3) * pow_2(AmbientDeltaT)) -
                          273;

            //                        UAG=UACGC(1,IPLCTR)*GTDSLCAP(IS,TypeIndex,IPLCTR)**      &
            //                            UACGC(2,IPLCTR)
            if (PLoad != 0.0) {
                UAtoCapRat = GTChiller(ChillerNum).UAtoCapCoef(1) * std::pow(GTEngineCapacity, GTChiller(ChillerNum).UAtoCapCoef(2));

                //     TSTACK = EXHAUST STACK TEMPERATURE, C.
                //                        TSTACK=TSATUR(IPLCTR)+(TEX-TSATUR(IPLCTR))/      &
                //                               EXP(UAG/(AMAX1(FEX,RMXKGC(IPLCTR)*        &
                //                               GTDSLCAP(IS,TypeIndex,IPLCTR)) * 1.047))

                DesignSteamSatTemp = GTChiller(ChillerNum).DesignSteamSatTemp;
                _ExhaustStackTemp =
                    DesignSteamSatTemp + (ExhaustTemp - DesignSteamSatTemp) /
                                             std::exp(UAtoCapRat / (max(ExhaustFlow, MaxExhaustperGTPower * GTEngineCapacity) * ExhaustCP));

                //                        EEX = AMAX1 ( FEX*1.047*(TEX-TSTACK),0.0d0)
                //                        ELUBE=PLOAD*(ELUBEGC(1,IPLCTR)+ELUBEGC(2,IPLCTR) &
                //                              *RLOAD+ELUBEGC(3,IPLCTR)*RLD2 )
            }

            if (GTChiller(ChillerNum).HeatRecActive) {
                QHeatRecLube = PLoad * (GTChiller(ChillerNum).HeatRecLubeEnergyCoef(1) + GTChiller(ChillerNum).HeatRecLubeEnergyCoef(2) * RL +
                                        GTChiller(ChillerNum).HeatRecLubeEnergyCoef(3) * RL2);

            } else {
                QHeatRecLube = 0.0;
            }

            //                        CHLRFUEL(TypeIndex) = CHLRFUEL(TypeIndex) + EFUEL * NUMOPR
            //                        EEXGC = EEXGC + EEX * NUMOPR
            //                        ELBEGC = ELBEGC + ELUBE * NUMOPR

            // Heat Recovery Loop -  lube recovered heat
            //   If lube is not present, then the energy should be 0 at this point
            // Thigh = Energy / (Mdot*Cp) + Tlow

            // Need to set the HeatRecRatio to 1.0 if it is not modified
            HeatRecRatio = 1.0;

            if (GTChiller(ChillerNum).HeatRecActive) {
                // This mdot is input specified mdot "Desired Flowrate", already set at node in init routine
                HeatRecMdot = Node(HeatRecInNode).MassFlowRate;
                HeatRecInTemp = Node(HeatRecInNode).Temp;
                HeatRecCp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(GTChiller(ChillerNum).HRLoopNum).FluidName,
                                                  HeatRecInTemp,
                                                  DataPlant::PlantLoop(GTChiller(ChillerNum).HRLoopNum).FluidIndex,
                                                  RoutineNameHeatRecovery);

                // Don't divide by zero
                if ((HeatRecMdot > 0.0) && (HeatRecCp > 0.0)) {
                    HeatRecOutTemp = (QHeatRecLube) / (HeatRecMdot * HeatRecCp) + HeatRecInTemp;
                } else {
                    HeatRecOutTemp = HeatRecInTemp;
                }

                // Now verify that the design flowrate was large enough to prevent phase change
                if (HeatRecOutTemp > GTChiller(ChillerNum).HeatRecMaxTemp) {
                    if (GTChiller(ChillerNum).HeatRecMaxTemp != HeatRecInTemp) {
                        MinHeatRecMdot = (QHeatRecLube) / (HeatRecCp * (GTChiller(ChillerNum).HeatRecMaxTemp - HeatRecInTemp));
                        if (MinHeatRecMdot < 0.0) MinHeatRecMdot = 0.0;
                    }

                    // Recalculate Outlet Temperature, with adjusted flowrate
                    if ((MinHeatRecMdot > 0.0) && (HeatRecCp > 0.0)) {
                        HeatRecOutTemp = (QHeatRecLube) / (MinHeatRecMdot * HeatRecCp) + HeatRecInTemp;
                        HeatRecRatio = HeatRecMdot / MinHeatRecMdot;
                    } else {
                        HeatRecOutTemp = HeatRecInTemp;
                        HeatRecRatio = 0.0;
                    }
                }

                QHeatRecLube *= HeatRecRatio;
            } else {
                HeatRecInTemp = 0.0;
                HeatRecMdot = 0.0;
                HeatRecCp = 0.0;
                HeatRecOutTemp = 0.0;
            }
        }

        GTChiller(ChillerNum).HeatRecInletTemp = HeatRecInTemp;
        GTChiller(ChillerNum).HeatRecOutletTemp = HeatRecOutTemp;
        GTChiller(ChillerNum).HeatRecMdot = HeatRecMdot;
        GTChiller(ChillerNum).HeatRecLubeEnergy = QHeatRecLube * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
        GTChiller(ChillerNum).HeatRecLubeRate = QHeatRecLube;
        GTChiller(ChillerNum).FuelEnergyIn = std::abs(FuelEnergyIn);

        FuelHeatingValue = GTChiller(ChillerNum).FuelHeatingValue;

        GTChillerReport(ChillerNum).FuelMassUsedRate = std::abs(FuelEnergyIn) / (FuelHeatingValue * KJtoJ);

        GTChiller(ChillerNum).ExhaustStackTemp = _ExhaustStackTemp;

        // Calculate Energy
        modCondenserEnergy = modQCondenser * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEnergy = modPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEvaporatorEnergy = modQEvaporator * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        // check for problems BG 9/12/06 (deal with observed negative energy results)
        if (modEnergy < 0.0) { // there is a serious problem

            if (GTChiller(ChillerNum).Base.CondenserType == WaterCooled) {
                // first check for run away condenser loop temps (only reason yet to be observed for this?)
                if (_CondInletTemp > 70.0) {
                    ShowSevereError("CalcGTChillerModel: Condenser loop inlet temperatures over 70.0 C for GTChiller=" +
                                    GTChiller(ChillerNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Condenser loop water temperatures are too high at" + RoundSigDigits(_CondInletTemp, 2));
                    ShowContinueError("Check input for condenser plant loop, especially cooling tower");
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));

                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            if (!WarmupFlag) {
                if (AvailNomCapRat < 0.0) { // apparently the real reason energy goes negative
                    ShowSevereError("CalcGTChillerModel: Capacity ratio below zero for GTChiller=" + GTChiller(ChillerNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Check input for Capacity Ratio Curve");
                    ShowContinueError("Condenser inlet temperature: " + RoundSigDigits(_CondInletTemp, 2));
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));
                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            // If makes it here, set limits, chiller can't have negative energy/power
            // proceeding silently for now but may want to throw error here
            modPower = 0.0;
            modEnergy = 0.0;
        }
    }

    void CalcConstCOPChillerModel(int const ChillNum,
                                  Real64 &MyLoad,
                                  bool const RunFlag,
                                  int const EquipFlowCtrl // Flow control mode for the equipment
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR         Dan Fisher
        //       DATE WRITTEN   Sept. 1998
        //       MODIFIED       Nov.-Dec. 2001, Jan. 2002, Richard Liesen
        //                      Feb. 2010, Chandan Sharma, FSEC. Added basin heater
        //                      Jun. 2016, Rongpeng Zhang, LBNL. Applied the chiller supply water temperature sensor fault model
        //                      Nov. 2016, Rongpeng Zhang, LBNL. Added Fouling Chiller fault
        //       RE-ENGINEERED  na

        static ObjexxFCL::gio::Fmt OutputFormat("(F6.2)");
        static std::string const RoutineName("CalcConstCOPChillerModel");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 EvapDeltaTemp;
        Real64 TempEvapOutSetPoint(0.0); // C - evaporator outlet temperature setpoint
        int EvapInletNode;
        int EvapOutletNode;
        int CondInletNode;
        int CondOutletNode;
        //  LOGICAL,SAVE           :: PossibleSubcooling=.FALSE.
        int LoopNum;
        int LoopSideNum;
        Real64 CurrentEndTime;                 // end time of time step for current simulation time step
        std::string OutputChar;         // character string for warning messages
        Real64 COP;                            // coefficient of performance
        Real64 Cp;                             // local for fluid specif heat, for evaporator
        Real64 CpCond;                         // local for fluid specif heat, for condenser
        Real64 ChillerNomCap;                  // chiller nominal capacity

        ChillerNomCap = ConstCOPChiller(ChillNum).Base.NomCap;
        EvapInletNode = ConstCOPChiller(ChillNum).Base.EvapInletNodeNum;
        EvapOutletNode = ConstCOPChiller(ChillNum).Base.EvapOutletNodeNum;
        CondInletNode = ConstCOPChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = ConstCOPChiller(ChillNum).Base.CondOutletNodeNum;
        COP = ConstCOPChiller(ChillNum).Base.COP;

        // If there is a fault of chiller fouling (zrp_Nov2016)
        if (ConstCOPChiller(ChillNum).Base.FaultyChillerFoulingFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = ConstCOPChiller(ChillNum).Base.FaultyChillerFoulingIndex;
            Real64 NomCap_ff = ChillerNomCap;
            Real64 COP_ff = COP;

            // calculate the Faulty Chiller Fouling Factor using fault information
            ConstCOPChiller(ChillNum).Base.FaultyChillerFoulingFactor = FaultsManager::FaultsChillerFouling(FaultIndex).CalFoulingFactor();

            // update the Chiller nominal capacity and COP at faulty cases
            ChillerNomCap = NomCap_ff * ConstCOPChiller(ChillNum).Base.FaultyChillerFoulingFactor;
            COP = COP_ff * ConstCOPChiller(ChillNum).Base.FaultyChillerFoulingFactor;
        }

        // set module level chiller inlet and temperature variables
        LoopNum = ConstCOPChiller(ChillNum).Base.CWLoopNum;
        LoopSideNum = ConstCOPChiller(ChillNum).Base.CWLoopSideNum;
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                if ((ConstCOPChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) ||
                    (DataPlant::PlantLoop(LoopNum)
                         .LoopSide(LoopSideNum)
                         .Branch(ConstCOPChiller(ChillNum).Base.CWBranchNum)
                         .Comp(ConstCOPChiller(ChillNum).Base.CWCompNum)
                         .CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                    (Node(EvapOutletNode).TempSetPoint != SensedNodeFlagValue)) {
                    TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPoint;
                } else {
                    TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPoint;
                }
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                if ((ConstCOPChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) ||
                    (DataPlant::PlantLoop(LoopNum)
                         .LoopSide(LoopSideNum)
                         .Branch(ConstCOPChiller(ChillNum).Base.CWBranchNum)
                         .Comp(ConstCOPChiller(ChillNum).Base.CWCompNum)
                         .CurOpSchemeType == DataPlant::CompSetPtBasedSchemeType) ||
                    (Node(EvapOutletNode).TempSetPointHi != SensedNodeFlagValue)) {
                    TempEvapOutSetPoint = Node(EvapOutletNode).TempSetPointHi;
                } else {
                    TempEvapOutSetPoint = Node(DataPlant::PlantLoop(LoopNum).TempSetPointNodeNum).TempSetPointHi;
                }
            }
        }

        // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
        if (ConstCOPChiller(ChillNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation)) {
            int FaultIndex = ConstCOPChiller(ChillNum).Base.FaultyChillerSWTIndex;
            Real64 EvapOutletTemp_ff = TempEvapOutSetPoint;

            // calculate the sensor offset using fault information
            ConstCOPChiller(ChillNum).Base.FaultyChillerSWTOffset = FaultsManager::FaultsChillerSWTSensor(FaultIndex).CalFaultOffsetAct();
            // update the TempEvapOutSetPoint
            TempEvapOutSetPoint = min(Node(EvapInletNode).Temp, EvapOutletTemp_ff - ConstCOPChiller(ChillNum).Base.FaultyChillerSWTOffset);
            ConstCOPChiller(ChillNum).Base.FaultyChillerSWTOffset = EvapOutletTemp_ff - TempEvapOutSetPoint;
        }

        EvapDeltaTemp = std::abs(Node(EvapInletNode).Temp - TempEvapOutSetPoint);

        // If no component demand, or chiller OFF, or Chiller type set to 'Passive' by free
        // cooling heat exchanger, then set condenser side flow and heat transfer rates set to zero
        if (MyLoad >= 0.0 || !RunFlag) {

            // If Chiller load is 0 or greater or chiller is not running then leave the subroutine.Before leaving
            // if the component control is SERIESACTIVE we set the component flow to inlet flow so that
            // flow resolver will not shut down the branch
            if (EquipFlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive || DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 1) {
                modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            } else {
                modEvapMassFlowRate = 0.0;
                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                     ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                     ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                     ConstCOPChiller(ChillNum).Base.CWCompNum);
            }
            if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
                if (DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum)
                        .LoopSide(ConstCOPChiller(ChillNum).Base.CDLoopSideNum)
                        .Branch(ConstCOPChiller(ChillNum).Base.CDBranchNum)
                        .Comp(ConstCOPChiller(ChillNum).Base.CDCompNum)
                        .FlowCtrl == DataBranchAirLoopPlant::ControlType_SeriesActive) {
                    modCondMassFlowRate = Node(CondInletNode).MassFlowRate;
                } else {
                    modCondMassFlowRate = 0.0;
                    PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                         CondInletNode,
                                         CondOutletNode,
                                         ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                         ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                         ConstCOPChiller(ChillNum).Base.CDBranchNum,
                                         ConstCOPChiller(ChillNum).Base.CDCompNum);
                }
            }

            modEvapOutletTemp = Node(EvapInletNode).Temp;
            modCondOutletTemp = Node(CondInletNode).Temp;

            modPower = 0.0;
            modQEvaporator = 0.0;
            modQCondenser = 0.0;
            modEnergy = 0.0;
            modEvaporatorEnergy = 0.0;
            modCondenserEnergy = 0.0;

            if (ConstCOPChiller(ChillNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(ConstCOPChiller(ChillNum).Base.BasinHeaterPowerFTempDiff,
                                     ConstCOPChiller(ChillNum).Base.BasinHeaterSchedulePtr,
                                     ConstCOPChiller(ChillNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }
            ConstCOPChiller(ChillNum).Base.PrintMessage = false;
            return;
        }

        //   calculate end time of current time step
        CurrentEndTime = DataGlobals::CurrentTime + DataHVACGlobals::SysTimeElapsed;

        //   Print warning messages only when valid and only for the first ocurrance. Let summary provide statistics.
        //   Wait for next time step to print warnings. If simulation iterates, print out
        //   the warning for the last iteration only. Must wait for next time step to accomplish this.
        //   If a warning occurs and the simulation down shifts, the warning is not valid.
        if (CurrentEndTime > ConstCOPChiller(ChillNum).Base.CurrentEndTimeLast && DataHVACGlobals::TimeStepSys >= ConstCOPChiller(ChillNum).Base.TimeStepSysLast) {
            if (ConstCOPChiller(ChillNum).Base.PrintMessage) {
                ++ConstCOPChiller(ChillNum).Base.MsgErrorCount;
                //       Show single warning and pass additional info to ShowRecurringWarningErrorAtEnd
                if (ConstCOPChiller(ChillNum).Base.MsgErrorCount < 2) {
                    ShowWarningError(ConstCOPChiller(ChillNum).Base.MsgBuffer1 + '.');
                    ShowContinueError(ConstCOPChiller(ChillNum).Base.MsgBuffer2);
                } else {
                    ShowRecurringWarningErrorAtEnd(ConstCOPChiller(ChillNum).Base.MsgBuffer1 + " error continues.",
                                                   ConstCOPChiller(ChillNum).Base.ErrCount1,
                                                   ConstCOPChiller(ChillNum).Base.MsgDataLast,
                                                   ConstCOPChiller(ChillNum).Base.MsgDataLast,
                                                   _,
                                                   "[C]",
                                                   "[C]");
                }
            }
        }

        //   save last system time step and last end time of current time step (used to determine if warning is valid)
        ConstCOPChiller(ChillNum).Base.TimeStepSysLast = DataHVACGlobals::TimeStepSys;
        ConstCOPChiller(ChillNum).Base.CurrentEndTimeLast = CurrentEndTime;

        // otherwise the chiller is running...

        if (ConstCOPChiller(ChillNum).Base.CondenserType == AirCooled) { // Condenser inlet temp = outdoor temp
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirDryBulb;
            //  Warn user if entering condenser temperature falls below 0C
            if (Node(CondInletNode).Temp < 0.0 && !WarmupFlag) {
                ConstCOPChiller(ChillNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                ConstCOPChiller(ChillNum).Base.MsgBuffer1 = "CalcConstCOPChillerModel - Chiller:ConstantCOP \"" +
                                                            ConstCOPChiller(ChillNum).Base.Name +
                                                            "\" - Air Cooled Condenser Inlet Temperature below 0C";
                ConstCOPChiller(ChillNum).Base.MsgBuffer2 = "... Outdoor Dry-bulb Condition = " + OutputChar +
                                                            " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                    General::CreateSysTimeIntervalString();
                ConstCOPChiller(ChillNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                ConstCOPChiller(ChillNum).Base.PrintMessage = false;
            }
        } else if (ConstCOPChiller(ChillNum).Base.CondenserType == EvapCooled) { // Condenser inlet temp = (outdoor wet bulb)
            Node(CondInletNode).Temp = Node(CondInletNode).OutAirWetBulb;
            //  Warn user if evap condenser wet bulb temperature falls below 10C
            if (Node(CondInletNode).Temp < 10.0 && !WarmupFlag) {
                ConstCOPChiller(ChillNum).Base.PrintMessage = true;
                ObjexxFCL::gio::write(OutputChar, OutputFormat) << Node(CondInletNode).Temp;
                ConstCOPChiller(ChillNum).Base.MsgBuffer1 = "CalcConstCOPChillerModel - Chiller:ConstantCOP \"" +
                                                            ConstCOPChiller(ChillNum).Base.Name +
                                                            "\" - Evap Cooled Condenser Inlet Temperature below 10C";
                ConstCOPChiller(ChillNum).Base.MsgBuffer2 = "... Outdoor Wet-bulb Condition = " + OutputChar +
                                                            " C. Occurrence info = " + DataEnvironment::EnvironmentName + ", " + DataEnvironment::CurMnDy + ' ' +
                    General::CreateSysTimeIntervalString();
                ConstCOPChiller(ChillNum).Base.MsgDataLast = Node(CondInletNode).Temp;
            } else {
                ConstCOPChiller(ChillNum).Base.PrintMessage = false;
            }
        } // End of the Air Cooled/Evap Cooled Logic block

        // Set condenser flow rate
        if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
            modCondMassFlowRate = ConstCOPChiller(ChillNum).Base.CondMassFlowRateMax;
            PlantUtilities::SetComponentFlowRate(modCondMassFlowRate,
                                 CondInletNode,
                                 CondOutletNode,
                                 ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                 ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                 ConstCOPChiller(ChillNum).Base.CDBranchNum,
                                 ConstCOPChiller(ChillNum).Base.CDCompNum);
            PlantUtilities::PullCompInterconnectTrigger(ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                        ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                        ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                        ConstCOPChiller(ChillNum).Base.CWCompNum,
                                        ConstCOPChiller(ChillNum).Base.CondMassFlowIndex,
                                        ConstCOPChiller(ChillNum).Base.CDLoopNum,
                                        ConstCOPChiller(ChillNum).Base.CDLoopSideNum,
                                        DataPlant::CriteriaType_MassFlowRate,
                                                        modCondMassFlowRate);

            if (modCondMassFlowRate < DataBranchAirLoopPlant::MassFlowTolerance) return;
        }

        // If FlowLock is True, the new resolved mdot is used to update Power, QEvap, Qcond, and
        // condenser side outlet temperature.

        Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidName,
                                   Node(EvapInletNode).Temp,
                                   DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).FluidIndex,
                                   RoutineName);

        if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0) {
            ConstCOPChiller(ChillNum).Base.PossibleSubcooling = false;
            modQEvaporator = std::abs(MyLoad);
            modPower = std::abs(MyLoad) / COP;

            // Either set the flow to the Constant value or caluclate the flow for the variable volume
            if ((ConstCOPChiller(ChillNum).Base.FlowMode == ConstantFlow) || (ConstCOPChiller(ChillNum).Base.FlowMode == NotModulated)) {

                // Start by assuming max (design) flow
                modEvapMassFlowRate = ConstCOPChiller(ChillNum).Base.EvapMassFlowRateMax;
                // Use SetComponentFlowRate to decide actual flow
                PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                     EvapInletNode,
                                     EvapOutletNode,
                                     ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                     ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                     ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                     ConstCOPChiller(ChillNum).Base.CWCompNum);
                // Evaluate delta temp based on actual flow rate
                if (modEvapMassFlowRate != 0.0) {
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                } else {
                    EvapDeltaTemp = 0.0;
                }
                // Evaluate outlet temp based on delta
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;

            } else if (ConstCOPChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated) {

                // Calculate the Delta Temp from the inlet temp to the chiller outlet setpoint
                {
                    auto const SELECT_CASE_var(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).LoopDemandCalcScheme);
                    if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                        EvapDeltaTemp = std::abs(Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPoint);
                    } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                        EvapDeltaTemp = std::abs(Node(EvapInletNode).Temp - Node(EvapOutletNode).TempSetPointHi);
                    }
                }

                if (EvapDeltaTemp > DeltaTempTol) {
                    modEvapMassFlowRate = std::abs(modQEvaporator / Cp / EvapDeltaTemp);
                    if ((modEvapMassFlowRate - ConstCOPChiller(ChillNum).Base.EvapMassFlowRateMax) > DataBranchAirLoopPlant::MassFlowTolerance)
                        ConstCOPChiller(ChillNum).Base.PossibleSubcooling = true;
                    // Check to see if the Maximum is exceeded, if so set to maximum
                    modEvapMassFlowRate = min(ConstCOPChiller(ChillNum).Base.EvapMassFlowRateMax, modEvapMassFlowRate);
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                         ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                         ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                         ConstCOPChiller(ChillNum).Base.CWCompNum);
                    {
                        auto const SELECT_CASE_var(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CWLoopNum).LoopDemandCalcScheme);
                        if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPoint;
                        } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                            modEvapOutletTemp = Node(EvapOutletNode).TempSetPointHi;
                        }
                    }
                } else {
                    // Try to request zero flow
                    modEvapMassFlowRate = 0.0;
                    // Use SetComponentFlowRate to decide actual flow
                    PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                         EvapInletNode,
                                         EvapOutletNode,
                                         ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                         ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                         ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                         ConstCOPChiller(ChillNum).Base.CWCompNum);
                    // No deltaT since component is not running
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            } // End of Constant or Variable Flow If Block for FlowLock = 0 (or making a flow request)

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (ConstCOPChiller(ChillNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = ConstCOPChiller(ChillNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = (ConstCOPChiller(ChillNum).Base.FlowMode == LeavingSetPointModulated);
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        ConstCOPChiller(ChillNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                // PartLoadRat = ( AvailChillerCap > 0.0 ) ? ( QEvaporator / AvailChillerCap ) : 0.0;
                // PartLoadRat = max( 0.0, min( PartLoadRat, MaxPartLoadRat ));
                // ChillerPartLoadRatio = PartLoadRat;
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

        } else { // If FlowLock is True

            modEvapMassFlowRate = Node(EvapInletNode).MassFlowRate;
            PlantUtilities::SetComponentFlowRate(modEvapMassFlowRate,
                                 EvapInletNode,
                                 EvapOutletNode,
                                 ConstCOPChiller(ChillNum).Base.CWLoopNum,
                                 ConstCOPChiller(ChillNum).Base.CWLoopSideNum,
                                 ConstCOPChiller(ChillNum).Base.CWBranchNum,
                                 ConstCOPChiller(ChillNum).Base.CWCompNum);
            //   Some other component set the flow to 0. No reason to continue with calculations.
            if (modEvapMassFlowRate == 0.0) {
                MyLoad = 0.0;
                if (ConstCOPChiller(ChillNum).Base.CondenserType == EvapCooled) {
                    CalcBasinHeaterPower(ConstCOPChiller(ChillNum).Base.BasinHeaterPowerFTempDiff,
                                         ConstCOPChiller(ChillNum).Base.BasinHeaterSchedulePtr,
                                         ConstCOPChiller(ChillNum).Base.BasinHeaterSetPointTemp,
                                         modBasinHeaterPower);
                }
                ConstCOPChiller(ChillNum).Base.PrintMessage = false;
                return;
            }

            // Recalculate the Delts Temp
            if (ConstCOPChiller(ChillNum).Base.PossibleSubcooling) {
                modQEvaporator = std::abs(MyLoad);
                EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                if (modEvapOutletTemp < Node(EvapOutletNode).TempMin) {
                    modEvapOutletTemp = Node(EvapOutletNode).TempMin;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            } else {
                EvapDeltaTemp = Node(EvapInletNode).Temp - TempEvapOutSetPoint;
                // Calculate the evaporator heat transfer at the specified flow which could have changed
                //  in the Flow Resolution step.
                modQEvaporator = std::abs(modEvapMassFlowRate * Cp * EvapDeltaTemp);
                modEvapOutletTemp = TempEvapOutSetPoint;
            }
            // Check that the Evap outlet temp honors both plant loop temp low limit and also the chiller low limit
            if (modEvapOutletTemp < Node(EvapOutletNode).TempMin) {
                if ((Node(EvapInletNode).Temp - Node(EvapOutletNode).TempMin) > DeltaTempTol) {
                    modEvapOutletTemp = Node(EvapOutletNode).TempMin;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                } else {
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                    EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
                    modQEvaporator = modEvapMassFlowRate * Cp * EvapDeltaTemp;
                }
            }
            // If load exceeds the distributed load set to the distributed load
            if (modQEvaporator > std::abs(MyLoad)) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = std::abs(MyLoad);
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }

            // If there is a fault of Chiller SWT Sensor (zrp_Jun2016)
            if (ConstCOPChiller(ChillNum).Base.FaultyChillerSWTFlag && (!WarmupFlag) && (!DataGlobals::DoingSizing) && (!DataGlobals::KickOffSimulation) &&
                (modEvapMassFlowRate > 0)) {
                // calculate directly affected variables at faulty case: EvapOutletTemp, EvapMassFlowRate, QEvaporator
                int FaultIndex = ConstCOPChiller(ChillNum).Base.FaultyChillerSWTIndex;
                bool VarFlowFlag = false;
                FaultsManager::FaultsChillerSWTSensor(FaultIndex)
                    .CalFaultChillerSWT(VarFlowFlag,
                                        ConstCOPChiller(ChillNum).Base.FaultyChillerSWTOffset,
                                        Cp,
                                        Node(EvapInletNode).Temp,
                                        modEvapOutletTemp,
                                        modEvapMassFlowRate,
                                        modQEvaporator);
                // update corresponding variables at faulty case
                EvapDeltaTemp = Node(EvapInletNode).Temp - modEvapOutletTemp;
            }

            // Checks QEvaporator on the basis of the machine limits.
            if (modQEvaporator > ChillerNomCap) {
                if (modEvapMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                    modQEvaporator = ChillerNomCap;
                    EvapDeltaTemp = modQEvaporator / modEvapMassFlowRate / Cp;
                    modEvapOutletTemp = Node(EvapInletNode).Temp - EvapDeltaTemp;
                } else {
                    modQEvaporator = 0.0;
                    modEvapOutletTemp = Node(EvapInletNode).Temp;
                }
            }
            // Calculate the Power consumption of the Const COP chiller which is a simplified calculation
            modPower = modQEvaporator / COP;
            if (modEvapMassFlowRate == 0.0) {
                modQEvaporator = 0.0;
                modEvapOutletTemp = Node(EvapInletNode).Temp;
                modPower = 0.0;
                ConstCOPChiller(ChillNum).Base.PrintMessage = false;
            }
            if (modQEvaporator == 0.0 && ConstCOPChiller(ChillNum).Base.CondenserType == EvapCooled) {
                CalcBasinHeaterPower(ConstCOPChiller(ChillNum).Base.BasinHeaterPowerFTempDiff,
                                     ConstCOPChiller(ChillNum).Base.BasinHeaterSchedulePtr,
                                     ConstCOPChiller(ChillNum).Base.BasinHeaterSetPointTemp,
                                     modBasinHeaterPower);
            }

        } // This is the end of the FlowLock Block

        // QCondenser is calculated the same for each type, but the power consumption should be different
        //  depending on the performance coefficients used for the chiller model.
        modQCondenser = modPower + modQEvaporator;

        // If not air or evap cooled then set to the condenser node that is attached to a cooling tower
        Real64 const CondInletTemp = Node(CondInletNode).Temp;

        if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
            CpCond = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidName,
                                                            CondInletTemp,
                                           DataPlant::PlantLoop(ConstCOPChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                           RoutineName);
            if (modCondMassFlowRate > DataBranchAirLoopPlant::MassFlowTolerance) {
                modCondOutletTemp = modQCondenser / modCondMassFlowRate / CpCond + CondInletTemp;
            } else {
                ShowSevereError("CalcConstCOPChillerModel: Condenser flow = 0, for CONST COP Chiller=" + ConstCOPChiller(ChillNum).Base.Name);
                ShowContinueErrorTimeStamp("");
            }
        } else { // Air Cooled or Evap Cooled
            //  Set condenser outlet temp to condenser inlet temp for Air Cooled or Evap Cooled
            //  since there is no CondMassFlowRate and would divide by zero
            modCondOutletTemp = CondInletTemp;
        }

        // Calculate Energy
        modCondenserEnergy = modQCondenser * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEnergy = modPower * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
        modEvaporatorEnergy = modQEvaporator * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        // check for problems BG 9/12/06 (deal with observed negative energy results)
        if (modEnergy < 0.0) { // there is a serious problem

            if (ConstCOPChiller(ChillNum).Base.CondenserType == WaterCooled) {
                // first check for run away condenser loop temps (only reason yet to be observed for this?)
                if (CondInletTemp > 70.0) {
                    ShowSevereError("CalcConstCOPChillerModel: Condenser loop inlet temperatures over 70.0 C for ConstCOPChiller=" +
                                    ConstCOPChiller(ChillNum).Base.Name);
                    ShowContinueErrorTimeStamp("");
                    ShowContinueError("Condenser loop water temperatures are too high at" + RoundSigDigits(CondInletTemp, 2));
                    ShowContinueError("Check input for condenser plant loop, especially cooling tower");
                    ShowContinueError("Evaporator inlet temperature: " + RoundSigDigits(Node(EvapInletNode).Temp, 2));

                    ShowFatalError("Program Terminates due to previous error condition");
                }
            }
            // If makes it here, set limits, chiller can't have negative energy/power
            // proceeding silently for now but may want to throw error here
            modPower = 0.0;
            modEnergy = 0.0;
        }
    }

    void CalcElectricChillerHeatRecovery(int const ChillNum,         // number of the current electric chiller being simulated
                                         Real64 &QCond,              // current condenser load
                                         Real64 const CondMassFlow,  // current condenser Mass Flow
                                         Real64 const _CondInletTemp, // current condenser Inlet Temp
                                         Real64 &QHeatRec            // amount of heat recovered
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Richard Liesen
        //       DATE WRITTEN:    January 2004

        // PURPOSE OF THIS SUBROUTINE:
        // Calculate the heat recovered from the chiller condenser

        // Locals
        Real64 _HeatRecInletTemp;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("ChillerHeatRecovery");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int CondInletNode;  // condenser inlet node number, water side
        int CondOutletNode; // condenser outlet node number, water side
        int HeatRecInNode;
        int HeatRecOutNode;
        Real64 QTotal;
        Real64 HeatRecMassFlowRate;
        Real64 TAvgIn;
        Real64 TAvgOut;
        Real64 CpHeatRec;
        Real64 CpCond;
        Real64 THeatRecSetPoint(0.0);
        Real64 QHeatRecToSetPoint;
        Real64 HeatRecHighInletLimit;

        // Begin routine
        HeatRecInNode = ElectricChiller(ChillNum).HeatRecInletNodeNum;
        HeatRecOutNode = ElectricChiller(ChillNum).HeatRecOutletNodeNum;
        CondInletNode = ElectricChiller(ChillNum).Base.CondInletNodeNum;
        CondOutletNode = ElectricChiller(ChillNum).Base.CondOutletNodeNum;
        _HeatRecInletTemp = Node(HeatRecInNode).Temp;
        HeatRecMassFlowRate = Node(HeatRecInNode).MassFlowRate;

        CpHeatRec = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).FluidName,
                                                           _HeatRecInletTemp,
                                          DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).FluidIndex,
                                          RoutineName);

        if (ElectricChiller(ChillNum).Base.CondenserType == WaterCooled) {
            CpCond = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidName,
                                                            _CondInletTemp,
                                           DataPlant::PlantLoop(ElectricChiller(ChillNum).Base.CDLoopNum).FluidIndex,
                                           RoutineName);
        } else {
            CpCond = Psychrometrics::PsyCpAirFnWTdb(Node(CondInletNode).HumRat, _CondInletTemp);
        }

        // Before we modify the QCondenser, the total or original value is transferred to QTot
        QTotal = QCond;

        if (ElectricChiller(ChillNum).HeatRecSetPointNodeNum == 0) { // use original algorithm that blends temps
            TAvgIn = (HeatRecMassFlowRate * CpHeatRec * _HeatRecInletTemp + CondMassFlow * CpCond * _CondInletTemp) /
                     (HeatRecMassFlowRate * CpHeatRec + CondMassFlow * CpCond);

            TAvgOut = QTotal / (HeatRecMassFlowRate * CpHeatRec + CondMassFlow * CpCond) + TAvgIn;

            QHeatRec = HeatRecMassFlowRate * CpHeatRec * (TAvgOut - _HeatRecInletTemp);
            QHeatRec = max(QHeatRec, 0.0); // ensure non negative
            // check if heat flow too large for physical size of bundle
            QHeatRec = min(QHeatRec, ElectricChiller(ChillNum).HeatRecMaxCapacityLimit);
        } else { // use new algorithm to meet setpoint
            {
                auto const SELECT_CASE_var(DataPlant::PlantLoop(ElectricChiller(ChillNum).HRLoopNum).LoopDemandCalcScheme);

                if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                    THeatRecSetPoint = Node(ElectricChiller(ChillNum).HeatRecSetPointNodeNum).TempSetPoint;
                } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                    THeatRecSetPoint = Node(ElectricChiller(ChillNum).HeatRecSetPointNodeNum).TempSetPointHi;
                }
            }

            QHeatRecToSetPoint = HeatRecMassFlowRate * CpHeatRec * (THeatRecSetPoint - _HeatRecInletTemp);
            QHeatRecToSetPoint = max(QHeatRecToSetPoint, 0.0);
            QHeatRec = min(QTotal, QHeatRecToSetPoint);
            // check if heat flow too large for physical size of bundle
            QHeatRec = min(QHeatRec, ElectricChiller(ChillNum).HeatRecMaxCapacityLimit);
        }
        // check if limit on inlet is present and exceeded.
        if (ElectricChiller(ChillNum).HeatRecInletLimitSchedNum > 0) {
            HeatRecHighInletLimit = ScheduleManager::GetCurrentScheduleValue(ElectricChiller(ChillNum).HeatRecInletLimitSchedNum);
            if (_HeatRecInletTemp > HeatRecHighInletLimit) { // shut down heat recovery
                QHeatRec = 0.0;
            }
        }

        QCond = QTotal - QHeatRec;

        // Calculate a new Heat Recovery Coil Outlet Temp
        if (HeatRecMassFlowRate > 0.0) {
            modHeatRecOutletTemp = QHeatRec / (HeatRecMassFlowRate * CpHeatRec) + _HeatRecInletTemp;
        } else {
            modHeatRecOutletTemp = _HeatRecInletTemp;
        }
    }

    void CalcEngineChillerHeatRec(int const ChillerNum,         // Chiller number
                                  Real64 const EnergyRecovered, // Amount of heat recovered
                                  Real64 &HeatRecRatio          // Max Heat recovery ratio
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Brandon Anderson
        //       DATE WRITTEN:    November 2000

        // PURPOSE OF THIS SUBROUTINE:
        // To perform heat recovery calculations and node updates

        // METHODOLOGY EMPLOYED: This routine is required for the heat recovery loop.
        // It works in conjunction with the Heat Recovery Manager, and the PlantWaterHeater.
        // The chiller sets the flow on the loop first by the input design flowrate and then
        // performs a check to verify that

        // SUBROUTINE PARAMETER DEFINITIONS:
        static std::string const RoutineName("ChillerHeatRecovery");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int HeatRecInNode;
        Real64 HeatRecMdot;
        Real64 MinHeatRecMdot(0.0);
        Real64 HeatRecInTemp;
        Real64 HeatRecOutTemp;
        Real64 HeatRecCp;

        // Load inputs to local structure
        HeatRecInNode = EngineDrivenChiller(ChillerNum).HeatRecInletNodeNum;

        // Need to set the HeatRecRatio to 1.0 if it is not modified
        HeatRecRatio = 1.0;

        //  !This mdot is input specified mdot "Desired Flowrate", already set in init routine
        HeatRecMdot = Node(HeatRecInNode).MassFlowRate;

        HeatRecInTemp = Node(HeatRecInNode).Temp;
        HeatRecCp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).HRLoopNum).FluidName,
                                                           EngineDrivenChiller(ChillerNum).modHeatRecInletTemp,
                                          DataPlant::PlantLoop(EngineDrivenChiller(ChillerNum).HRLoopNum).FluidIndex,
                                          RoutineName);

        // Don't divide by zero - Note This also results in no heat recovery when
        //  design Mdot for Heat Recovery - Specified on Chiller Input - is zero
        //  In order to see what minimum heat recovery flow rate is for the design temperature
        //  The design heat recovery flow rate can be set very small, but greater than zero.
        if ((HeatRecMdot > 0) && (HeatRecCp > 0)) {
            HeatRecOutTemp = (EnergyRecovered) / (HeatRecMdot * HeatRecCp) + HeatRecInTemp;
        } else {
            HeatRecOutTemp = HeatRecInTemp;
        }

        // Now verify that the design flowrate was large enough to prevent phase change
        if (HeatRecOutTemp > EngineDrivenChiller(ChillerNum).HeatRecMaxTemp) {
            if (EngineDrivenChiller(ChillerNum).HeatRecMaxTemp != HeatRecInTemp) {
                MinHeatRecMdot = (EnergyRecovered) / (HeatRecCp * (EngineDrivenChiller(ChillerNum).HeatRecMaxTemp - HeatRecInTemp));
                if (MinHeatRecMdot < 0.0) MinHeatRecMdot = 0.0;
            }

            // Recalculate Outlet Temperature, with adjusted flowrate
            if ((MinHeatRecMdot > 0.0) && (HeatRecCp > 0.0)) {
                HeatRecOutTemp = (EnergyRecovered) / (MinHeatRecMdot * HeatRecCp) + HeatRecInTemp;
                HeatRecRatio = HeatRecMdot / MinHeatRecMdot;
            } else {
                HeatRecOutTemp = HeatRecInTemp;
                HeatRecRatio = 0.0;
            }
        }

        // Update global variables for reporting later
        EngineDrivenChiller(ChillerNum).modHeatRecInletTemp = HeatRecInTemp;
        modHeatRecOutletTemp = HeatRecOutTemp;
        EngineDrivenChiller(ChillerNum).modHeatRecMdotActual = HeatRecMdot;
    }

    void UpdateElectricChillerRecords(Real64 const MyLoad, // current load
                                      bool const RunFlag,  // TRUE if chiller operating
                                      int const Num        // chiller number
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher / Brandon Anderson
        //       DATE WRITTEN:    September 2000

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int EvapInletNode;  // evaporator inlet node number, water side
        int EvapOutletNode; // evaporator outlet node number, water side
        int CondInletNode;  // condenser inlet node number, water side
        int CondOutletNode; // condenser outlet node number, water side
        int HeatRecInNode;
        int HeatRecOutNode;
        Real64 ReportingConstant; // Number of seconds per HVAC system time step, to convert from W (J/s) to J

        ReportingConstant = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        EvapInletNode = ElectricChiller(Num).Base.EvapInletNodeNum;
        EvapOutletNode = ElectricChiller(Num).Base.EvapOutletNodeNum;
        CondInletNode = ElectricChiller(Num).Base.CondInletNodeNum;
        CondOutletNode = ElectricChiller(Num).Base.CondOutletNodeNum;
        HeatRecInNode = ElectricChiller(Num).HeatRecInletNodeNum;
        HeatRecOutNode = ElectricChiller(Num).HeatRecOutletNodeNum;

        if (MyLoad >= 0.0 || !RunFlag) { // Chiller not running so pass inlet states to outlet states
            // set node temperatures
            Node(EvapOutletNode).Temp = Node(EvapInletNode).Temp;
            Node(CondOutletNode).Temp = Node(CondInletNode).Temp;
            if (ElectricChiller(Num).Base.CondenserType != WaterCooled) {
                Node(CondOutletNode).HumRat = Node(CondInletNode).HumRat;
                Node(CondOutletNode).Enthalpy = Node(CondInletNode).Enthalpy;
            }

            ElectricChillerReport(Num).Base.Power = 0.0;
            ElectricChillerReport(Num).Base.QEvap = 0.0;
            ElectricChillerReport(Num).Base.QCond = 0.0;
            ElectricChillerReport(Num).Base.Energy = 0.0;
            ElectricChillerReport(Num).Base.EvapEnergy = 0.0;
            ElectricChillerReport(Num).Base.CondEnergy = 0.0;
            ElectricChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            ElectricChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            ElectricChillerReport(Num).Base.CondOutletTemp = Node(CondOutletNode).Temp;
            ElectricChillerReport(Num).Base.EvapOutletTemp = Node(EvapOutletNode).Temp;
            ElectricChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            ElectricChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            ElectricChillerReport(Num).ActualCOP = 0.0;
            if (ElectricChiller(Num).Base.CondenserType == EvapCooled) {
                ElectricChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                ElectricChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }

            if (ElectricChiller(Num).HeatRecActive) {

                PlantUtilities::SafeCopyPlantNode(HeatRecInNode, HeatRecOutNode);

                ElectricChillerReport(Num).QHeatRecovery = 0.0;
                ElectricChillerReport(Num).EnergyHeatRecovery = 0.0;
                ElectricChillerReport(Num).HeatRecInletTemp = Node(HeatRecInNode).Temp;
                ElectricChillerReport(Num).HeatRecOutletTemp = Node(HeatRecOutNode).Temp;
                ElectricChillerReport(Num).HeatRecMassFlow = Node(HeatRecInNode).MassFlowRate;

                ElectricChillerReport(Num).ChillerCondAvgTemp = modAvgCondSinkTemp;
            }

        } else { // Chiller is running, so pass calculated values
            // set node temperatures
            Node(EvapOutletNode).Temp = modEvapOutletTemp;
            Node(CondOutletNode).Temp = modCondOutletTemp;
            if (ElectricChiller(Num).Base.CondenserType != WaterCooled) {
                Node(CondOutletNode).HumRat = modCondOutletHumRat;
                Node(CondOutletNode).Enthalpy = Psychrometrics::PsyHFnTdbW(modCondOutletTemp, modCondOutletHumRat);
            }
            // set node flow rates;  for these load based models
            // assume that the sufficient evaporator flow rate available
            ElectricChillerReport(Num).Base.Power = modPower;
            ElectricChillerReport(Num).Base.QEvap = modQEvaporator;
            ElectricChillerReport(Num).Base.QCond = modQCondenser;
            ElectricChillerReport(Num).Base.Energy = modEnergy;
            ElectricChillerReport(Num).Base.EvapEnergy = modEvaporatorEnergy;
            ElectricChillerReport(Num).Base.CondEnergy = modCondenserEnergy;
            ElectricChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            ElectricChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            ElectricChillerReport(Num).Base.CondOutletTemp = Node(CondOutletNode).Temp;
            ElectricChillerReport(Num).Base.EvapOutletTemp = Node(EvapOutletNode).Temp;
            ElectricChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            ElectricChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            if (ElectricChiller(Num).Base.CondenserType == EvapCooled) {
                ElectricChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                ElectricChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }
            if (modPower != 0.0) {
                ElectricChillerReport(Num).ActualCOP = modQEvaporator / modPower;
            } else {
                ElectricChillerReport(Num).ActualCOP = 0.0;
            }

            if (ElectricChiller(Num).HeatRecActive) {

                PlantUtilities::SafeCopyPlantNode(HeatRecInNode, HeatRecOutNode);
                ElectricChillerReport(Num).QHeatRecovery = modQHeatRecovered;
                ElectricChillerReport(Num).EnergyHeatRecovery = modQHeatRecovered * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
                Node(HeatRecOutNode).Temp = modHeatRecOutletTemp;
                ElectricChillerReport(Num).HeatRecInletTemp = Node(HeatRecInNode).Temp;
                ElectricChillerReport(Num).HeatRecOutletTemp = Node(HeatRecOutNode).Temp;
                ElectricChillerReport(Num).HeatRecMassFlow = Node(HeatRecInNode).MassFlowRate;
                ElectricChillerReport(Num).ChillerCondAvgTemp = modAvgCondSinkTemp;
            }
        }
    }

    void UpdateEngineDrivenChiller(Real64 const MyLoad, // current load
                                   bool const RunFlag,  // TRUE if chiller operating
                                   int const Num        // chiller number
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher / Brandon Anderson
        //       DATE WRITTEN:    September 2000

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int EvapInletNode;  // evaporator inlet node number, water side
        int EvapOutletNode; // evaporator outlet node number, water side
        int CondInletNode;  // condenser inlet node number, water side
        int CondOutletNode; // condenser outlet node number, water side
        int HeatRecInletNode;
        int HeatRecOutletNode;
        Real64 ReportingConstant; // Number of seconds per HVAC system time step, to convert from W (J/s) to J

        ReportingConstant = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        EvapInletNode = EngineDrivenChiller(Num).Base.EvapInletNodeNum;
        EvapOutletNode = EngineDrivenChiller(Num).Base.EvapOutletNodeNum;
        CondInletNode = EngineDrivenChiller(Num).Base.CondInletNodeNum;
        CondOutletNode = EngineDrivenChiller(Num).Base.CondOutletNodeNum;

        HeatRecInletNode = EngineDrivenChiller(Num).HeatRecInletNodeNum;
        HeatRecOutletNode = EngineDrivenChiller(Num).HeatRecOutletNodeNum;

        if (MyLoad >= 0.0 || !RunFlag) { // Chiller not running
            // set node temperatures
            Node(EvapOutletNode).Temp = Node(EvapInletNode).Temp;
            Node(CondOutletNode).Temp = Node(CondInletNode).Temp;

            EngineDrivenChillerReport(Num).Base.Power = 0.0;
            EngineDrivenChillerReport(Num).Base.QEvap = 0.0;
            EngineDrivenChillerReport(Num).Base.QCond = 0.0;
            EngineDrivenChillerReport(Num).Base.Energy = 0.0;
            EngineDrivenChillerReport(Num).Base.EvapEnergy = 0.0;
            EngineDrivenChillerReport(Num).Base.CondEnergy = 0.0;
            EngineDrivenChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            EngineDrivenChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            EngineDrivenChillerReport(Num).Base.CondOutletTemp = Node(CondOutletNode).Temp;
            EngineDrivenChillerReport(Num).Base.EvapOutletTemp = Node(EvapOutletNode).Temp;
            EngineDrivenChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            EngineDrivenChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            EngineDrivenChillerReport(Num).FuelCOP = 0.0;
            if (EngineDrivenChiller(Num).Base.CondenserType == EvapCooled) {
                EngineDrivenChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                EngineDrivenChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }
        } else { // Chiller is running
            // set node temperatures
            Node(EvapOutletNode).Temp = modEvapOutletTemp;
            Node(CondOutletNode).Temp = modCondOutletTemp;

            EngineDrivenChillerReport(Num).Base.Power = modPower;
            EngineDrivenChillerReport(Num).Base.QEvap = modQEvaporator;
            EngineDrivenChillerReport(Num).Base.QCond = modQCondenser;
            EngineDrivenChillerReport(Num).Base.Energy = modEnergy;
            EngineDrivenChillerReport(Num).Base.EvapEnergy = modEvaporatorEnergy;
            EngineDrivenChillerReport(Num).Base.CondEnergy = modCondenserEnergy;
            EngineDrivenChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            EngineDrivenChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            EngineDrivenChillerReport(Num).Base.CondOutletTemp = Node(CondOutletNode).Temp;
            EngineDrivenChillerReport(Num).Base.EvapOutletTemp = Node(EvapOutletNode).Temp;
            EngineDrivenChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            EngineDrivenChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            if (EngineDrivenChiller(Num).modFuelEnergyUseRate != 0.0) {
                EngineDrivenChillerReport(Num).FuelCOP = modQEvaporator / EngineDrivenChiller(Num).modFuelEnergyUseRate;
            } else {
                EngineDrivenChillerReport(Num).FuelCOP = 0.0;
            }
            if (EngineDrivenChiller(Num).Base.CondenserType == EvapCooled) {
                EngineDrivenChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                EngineDrivenChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }
        }

        // Update Heat Recovery Stuff whether running or not, variables should be set correctly
        EngineDrivenChillerReport(Num).QJacketRecovered = EngineDrivenChiller(Num).modQJacketRecovered;
        EngineDrivenChillerReport(Num).QLubeOilRecovered = EngineDrivenChiller(Num).modQLubeOilRecovered;
        EngineDrivenChillerReport(Num).QExhaustRecovered = EngineDrivenChiller(Num).modQExhaustRecovered;
        EngineDrivenChillerReport(Num).QTotalHeatRecovered = EngineDrivenChiller(Num).modQTotalHeatRecovered;
        EngineDrivenChillerReport(Num).FuelEnergyUseRate = EngineDrivenChiller(Num).modFuelEnergyUseRate;
        EngineDrivenChillerReport(Num).JacketEnergyRec = EngineDrivenChiller(Num).modJacketEnergyRec;
        EngineDrivenChillerReport(Num).LubeOilEnergyRec = EngineDrivenChiller(Num).modLubeOilEnergyRec;
        EngineDrivenChillerReport(Num).ExhaustEnergyRec = EngineDrivenChiller(Num).modExhaustEnergyRec;
        EngineDrivenChillerReport(Num).TotalHeatEnergyRec = EngineDrivenChiller(Num).modTotalHeatEnergyRec;
        EngineDrivenChillerReport(Num).FuelEnergy = EngineDrivenChiller(Num).modFuelEnergy;
        EngineDrivenChillerReport(Num).FuelMdot = EngineDrivenChiller(Num).modFuelMdot;
        EngineDrivenChillerReport(Num).ExhaustStackTemp = EngineDrivenChiller(Num).modExhaustStackTemp;
        EngineDrivenChillerReport(Num).HeatRecInletTemp = EngineDrivenChiller(Num).modHeatRecInletTemp;
        EngineDrivenChillerReport(Num).HeatRecOutletTemp = modHeatRecOutletTemp;
        EngineDrivenChillerReport(Num).HeatRecMdot = EngineDrivenChiller(Num).modHeatRecMdotActual;

        // Update the Heat Recovery outlet
        if (EngineDrivenChiller(Num).HeatRecActive) {
            PlantUtilities::SafeCopyPlantNode(HeatRecInletNode, HeatRecOutletNode);
            Node(HeatRecOutletNode).Temp = modHeatRecOutletTemp;
        }
    }

    void UpdateGTChillerRecords(Real64 const MyLoad, // current load
                                bool const RunFlag,  // TRUE if chiller operating
                                int const Num        // chiller number
    )
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher / Brandon Anderson
        //       DATE WRITTEN:    September 2000

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int EvapInletNode;  // evaporator inlet node number, water side
        int EvapOutletNode; // evaporator outlet node number, water side
        int CondInletNode;  // condenser inlet node number, water side
        int CondOutletNode; // condenser outlet node number, water side

        int HeatRecInletNode;
        int HeatRecOutletNode;
        Real64 ReportingConstant; // Number of seconds per HVAC system time step, to convert from W (J/s) to J

        ReportingConstant = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        EvapInletNode = GTChiller(Num).Base.EvapInletNodeNum;
        EvapOutletNode = GTChiller(Num).Base.EvapOutletNodeNum;
        CondInletNode = GTChiller(Num).Base.CondInletNodeNum;
        CondOutletNode = GTChiller(Num).Base.CondOutletNodeNum;
        if (GTChiller(Num).HeatRecActive) {
            HeatRecInletNode = GTChiller(Num).HeatRecInletNodeNum;
            HeatRecOutletNode = GTChiller(Num).HeatRecOutletNodeNum;
        }

        if (MyLoad >= 0.0 || !RunFlag) { // Chiller not running so pass inlet states to outlet states
            // set node temperatures
            Node(EvapOutletNode).Temp = Node(EvapInletNode).Temp;
            Node(CondOutletNode).Temp = Node(CondInletNode).Temp;

            if (GTChiller(Num).HeatRecActive) {
                PlantUtilities::SafeCopyPlantNode(HeatRecOutletNode, HeatRecInletNode);
                GTChillerReport(Num).HeatRecInletTemp = Node(HeatRecInletNode).Temp;
                GTChillerReport(Num).HeatRecOutletTemp = Node(HeatRecOutletNode).Temp;
            }

            GTChillerReport(Num).Base.Power = 0.0;
            GTChillerReport(Num).Base.QEvap = 0.0;
            GTChillerReport(Num).Base.QCond = 0.0;
            GTChillerReport(Num).Base.Energy = 0.0;
            GTChillerReport(Num).Base.EvapEnergy = 0.0;
            GTChillerReport(Num).Base.CondEnergy = 0.0;
            GTChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            GTChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            GTChillerReport(Num).Base.CondOutletTemp = Node(CondOutletNode).Temp;
            GTChillerReport(Num).Base.EvapOutletTemp = Node(EvapOutletNode).Temp;
            GTChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            GTChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            GTChillerReport(Num).FuelEnergyUsedRate = 0.0;
            GTChillerReport(Num).FuelMassUsedRate = 0.0;
            GTChillerReport(Num).FuelEnergyUsed = 0.0;
            GTChillerReport(Num).FuelMassUsed = 0.0;

            GTChillerReport(Num).HeatRecLubeEnergy = 0.0;
            GTChillerReport(Num).HeatRecLubeRate = 0.0;
            GTChillerReport(Num).ExhaustStackTemp = 0.0;
            GTChillerReport(Num).HeatRecMdot = GTChiller(Num).HeatRecMdot;
            GTChillerReport(Num).FuelCOP = 0.0;
            if (GTChiller(Num).Base.CondenserType == EvapCooled) {
                GTChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                GTChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }

        } else { // Chiller is running so report calculated values
            // set node temperatures
            Node(EvapOutletNode).Temp = modEvapOutletTemp;
            Node(CondOutletNode).Temp = modCondOutletTemp;

            if (GTChiller(Num).HeatRecActive) {
                PlantUtilities::SafeCopyPlantNode(HeatRecOutletNode, HeatRecInletNode);
                Node(HeatRecOutletNode).Temp = GTChiller(Num).HeatRecOutletTemp;
            }

            GTChillerReport(Num).Base.Power = modPower;
            GTChillerReport(Num).Base.QEvap = modQEvaporator;
            GTChillerReport(Num).Base.QCond = modQCondenser;
            GTChillerReport(Num).Base.Energy = modEnergy;
            GTChillerReport(Num).Base.EvapEnergy = modEvaporatorEnergy;
            GTChillerReport(Num).Base.CondEnergy = modCondenserEnergy;
            GTChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            GTChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            GTChillerReport(Num).Base.CondOutletTemp = Node(CondOutletNode).Temp;
            GTChillerReport(Num).Base.EvapOutletTemp = Node(EvapOutletNode).Temp;
            GTChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            GTChillerReport(Num).Base.Condmdot = modCondMassFlowRate;

            GTChillerReport(Num).HeatRecLubeEnergy = GTChiller(Num).HeatRecLubeEnergy;
            GTChillerReport(Num).HeatRecLubeRate = GTChiller(Num).HeatRecLubeRate;
            GTChillerReport(Num).FuelEnergyUsedRate = GTChiller(Num).FuelEnergyIn;
            GTChillerReport(Num).FuelMassUsedRate = GTChillerReport(Num).FuelMassUsedRate;
            GTChillerReport(Num).FuelEnergyUsed = GTChillerReport(Num).FuelEnergyUsedRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
            GTChillerReport(Num).FuelMassUsed = GTChillerReport(Num).FuelMassUsedRate * DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;
            GTChillerReport(Num).ExhaustStackTemp = GTChiller(Num).ExhaustStackTemp;
            GTChillerReport(Num).HeatRecInletTemp = GTChiller(Num).HeatRecInletTemp;
            GTChillerReport(Num).HeatRecOutletTemp = GTChiller(Num).HeatRecOutletTemp;
            GTChillerReport(Num).HeatRecMdot = GTChiller(Num).HeatRecMdot;
            if (GTChillerReport(Num).FuelEnergyUsedRate != 0.0) {
                GTChillerReport(Num).FuelCOP = GTChillerReport(Num).Base.QEvap / GTChillerReport(Num).FuelEnergyUsedRate;
            } else {
                GTChillerReport(Num).FuelCOP = 0.0;
            }
            if (GTChiller(Num).Base.CondenserType == EvapCooled) {
                GTChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                GTChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }
        }
    }

    void UpdateConstCOPChillerRecords(Real64 const MyLoad, // unused1208
                                      bool const RunFlag,  // unused1208
                                      int const Num)
    {
        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Dan Fisher
        //       DATE WRITTEN:    October 1998

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int EvapInletNode;
        int EvapOutletNode;
        int CondInletNode;
        int CondOutletNode;
        Real64 ReportingConstant; // Number of seconds per HVAC system time step, to convert from W (J/s) to J

        ReportingConstant = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        EvapInletNode = ConstCOPChiller(Num).Base.EvapInletNodeNum;
        EvapOutletNode = ConstCOPChiller(Num).Base.EvapOutletNodeNum;
        CondInletNode = ConstCOPChiller(Num).Base.CondInletNodeNum;
        CondOutletNode = ConstCOPChiller(Num).Base.CondOutletNodeNum;

        if (MyLoad >= 0.0 || !RunFlag) { // Chiller not running so pass inlet states to outlet states
            ConstCOPChillerReport(Num).Base.Power = 0.0;
            ConstCOPChillerReport(Num).Base.QEvap = 0.0;
            ConstCOPChillerReport(Num).Base.QCond = 0.0;
            ConstCOPChillerReport(Num).Base.Energy = 0.0;
            ConstCOPChillerReport(Num).Base.EvapEnergy = 0.0;
            ConstCOPChillerReport(Num).Base.CondEnergy = 0.0;
            ConstCOPChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            ConstCOPChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            ConstCOPChillerReport(Num).Base.CondOutletTemp = Node(CondInletNode).Temp;
            ConstCOPChillerReport(Num).Base.EvapOutletTemp = Node(EvapInletNode).Temp;
            ConstCOPChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            ConstCOPChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            ConstCOPChillerReport(Num).ActualCOP = 0.0;
            if (ConstCOPChiller(Num).Base.CondenserType == EvapCooled) {
                ConstCOPChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                ConstCOPChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }

            // set outlet node temperatures
            Node(EvapOutletNode).Temp = Node(EvapInletNode).Temp;
            Node(CondOutletNode).Temp = Node(CondInletNode).Temp;

        } else {
            ConstCOPChillerReport(Num).Base.Power = modPower;
            ConstCOPChillerReport(Num).Base.QEvap = modQEvaporator;
            ConstCOPChillerReport(Num).Base.QCond = modQCondenser;
            ConstCOPChillerReport(Num).Base.Energy = modEnergy;
            ConstCOPChillerReport(Num).Base.EvapEnergy = modEvaporatorEnergy;
            ConstCOPChillerReport(Num).Base.CondEnergy = modCondenserEnergy;
            ConstCOPChillerReport(Num).Base.CondInletTemp = Node(CondInletNode).Temp;
            ConstCOPChillerReport(Num).Base.EvapInletTemp = Node(EvapInletNode).Temp;
            ConstCOPChillerReport(Num).Base.CondOutletTemp = modCondOutletTemp;
            ConstCOPChillerReport(Num).Base.EvapOutletTemp = modEvapOutletTemp;
            ConstCOPChillerReport(Num).Base.Evapmdot = modEvapMassFlowRate;
            ConstCOPChillerReport(Num).Base.Condmdot = modCondMassFlowRate;
            if (modPower != 0.0) {
                ConstCOPChillerReport(Num).ActualCOP = modQEvaporator / modPower;
            } else {
                ConstCOPChillerReport(Num).ActualCOP = 0.0;
            }
            if (ConstCOPChiller(Num).Base.CondenserType == EvapCooled) {
                ConstCOPChillerReport(Num).Base.BasinHeaterPower = modBasinHeaterPower;
                ConstCOPChillerReport(Num).Base.BasinHeaterConsumption = modBasinHeaterPower * ReportingConstant;
            }

            // set outlet node temperatures
            Node(EvapOutletNode).Temp = modEvapOutletTemp;
            Node(CondOutletNode).Temp = modCondOutletTemp;
        }
    }

} // namespace PlantChillers

} // namespace EnergyPlus
