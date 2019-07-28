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

#include <Scheduling/Base.hh>
#include <Scheduling/Week.hh>
#include <Scheduling/Day.hh>
#include <UtilityRoutines.hh>
#include <InputProcessing/InputProcessor.hh>

#include <nlohmann/json.hpp>

namespace Scheduling {

bool getInputFlag = true;
std::vector<ScheduleWeekDaily> scheduleWeekDailies;
std::vector<ScheduleWeekCompact> scheduleWeekCompacts;

void ScheduleWeek::clear_state()
{
    getInputFlag = true;
    scheduleWeekDailies.clear();
    scheduleWeekCompacts.clear();
}

ScheduleWeek *ScheduleWeek::factory(const std::string& scheduleName)
{
    // get input if needed
    if (getInputFlag) {
        ScheduleWeek::processInput();
    }
    // then loop over supported vectors to try to find it
    for (auto & dailySchedule : scheduleWeekDailies) {
        if (dailySchedule.name == scheduleName) {
            return &dailySchedule;
        }
    }
    for (auto & compactSchedule : scheduleWeekCompacts) {
        if (compactSchedule.name == scheduleName) {
            return &compactSchedule;
        }
    }
    // error if not found
//    EnergyPlus::ShowFatalError("Could not find week schedule named \"" + scheduleName + "\"");
    return nullptr;
}
void ScheduleWeek::processInput()
{
    {
        std::string thisObjectType = "Schedule:Week:Daily";
        auto instances = EnergyPlus::inputProcessor->epJSON.find(thisObjectType);
        if (instances != EnergyPlus::inputProcessor->epJSON.end()) {
            auto &instancesValue = instances.value();
            for (auto instance = instancesValue.begin(); instance != instancesValue.end(); ++instance) {
                auto const &fields = instance.value();
                auto const &thisObjectName = EnergyPlus::UtilityRoutines::MakeUPPERCase(instance.key());
                // do any pre-construction operations
                EnergyPlus::inputProcessor->markObjectAsUsed(thisObjectType, thisObjectName);
                if (std::find(Scheduling::allSchedNames.begin(), Scheduling::allSchedNames.end(), thisObjectName) !=
                    Scheduling::allSchedNames.end()) {
                    EnergyPlus::ShowFatalError("Duplicate schedule name, all schedules, across all schedule types, must be uniquely named");
                }
                // then just add it to the vector via the constructor
                scheduleWeekDailies.emplace_back(thisObjectName, fields);
            }
        }
    }
    {
        std::string thisObjectType = "Schedule:Week:Compact";
        auto instances = EnergyPlus::inputProcessor->epJSON.find(thisObjectType);
        if (instances != EnergyPlus::inputProcessor->epJSON.end()) {
            auto &instancesValue = instances.value();
            for (auto instance = instancesValue.begin(); instance != instancesValue.end(); ++instance) {
                auto const &fields = instance.value();
                auto const &thisObjectName = EnergyPlus::UtilityRoutines::MakeUPPERCase(instance.key());
                // do any pre-construction operations
                EnergyPlus::inputProcessor->markObjectAsUsed(thisObjectType, thisObjectName);
                if (std::find(Scheduling::allSchedNames.begin(), Scheduling::allSchedNames.end(), thisObjectName) !=
                    Scheduling::allSchedNames.end()) {
                    EnergyPlus::ShowFatalError("Duplicate schedule name, all schedules, across all schedule types, must be uniquely named");
                }
                // then just add it to the vector via the constructor
                scheduleWeekCompacts.emplace_back(thisObjectName, fields);
            }
        }
    }
    // get schedule:week:daily and schedule:week:compact, fill appropriate vectors
    getInputFlag = false;
}

ScheduleWeekDaily::ScheduleWeekDaily(std::string const &objectName, nlohmann::json const &fields)
{
    auto & uc = EnergyPlus::UtilityRoutines::MakeUPPERCase;
    this->name = EnergyPlus::UtilityRoutines::MakeUPPERCase(objectName);
    this->sunday = ScheduleDay::factory(uc(fields.at("sunday_schedule_day_name")));
    this->monday = ScheduleDay::factory(uc(fields.at("monday_schedule_day_name")));
    this->tuesday = ScheduleDay::factory(uc(fields.at("tuesday_schedule_day_name")));
    this->wednesday = ScheduleDay::factory(uc(fields.at("wednesday_schedule_day_name")));
    this->thursday = ScheduleDay::factory(uc(fields.at("thursday_schedule_day_name")));
    this->friday = ScheduleDay::factory(uc(fields.at("friday_schedule_day_name")));
    this->saturday = ScheduleDay::factory(uc(fields.at("saturday_schedule_day_name")));
    this->holiday = ScheduleDay::factory(uc(fields.at("holiday_schedule_day_name")));
    this->summerDesignDay = ScheduleDay::factory(uc(fields.at("summerdesignday_schedule_day_name")));
    this->winterDesignDay = ScheduleDay::factory(uc(fields.at("winterdesignday_schedule_day_name")));
    this->customDay1 = ScheduleDay::factory(uc(fields.at("customday1_schedule_day_name")));
    this->customDay2 = ScheduleDay::factory(uc(fields.at("customday2_schedule_day_name")));
}

ScheduleWeekCompact::ScheduleWeekCompact(std::string const &objectName, nlohmann::json const &fields)
{
    this->name = EnergyPlus::UtilityRoutines::MakeUPPERCase(objectName);
    auto & daysData = fields.at("data");
    for (auto const & dayData : daysData) {
        this->dayTypeList.push_back(dayData.at("daytype_list"));
        this->scheduleDayName.push_back(dayData.at("schedule_day_name"));
        // auto scheduleInstance = ScheduleWeek::factory(scheduleName);
        //weekScheduleRanges.emplace_back(startMonth, startDay, endMonth, endDay, scheduleInstance);
    }
}
} // namespace Scheduling