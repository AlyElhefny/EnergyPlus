#include <gtest/gtest.h>
#include "../EPFMI.hpp"
#include "config.hpp"

TEST( EPFMI, Alpha ) {
  int result = instantiate(input, // input
                           weather, // weather
                           idd, // idd
                           "Alpha", // instanceName
                           nullptr, // varNames
                           nullptr, // varPointers
                           0, // nVars
                           nullptr); // log

  double tStart = 0.0;
  bool stopTimeDefined = true;
  double tEnd = 86400;

  result = setupExperiment(tStart, stopTimeDefined, tEnd, nullptr);

  fmiEventInfo eventInfo;
  double time = tStart;

  double outputs[] = {0.0};
  const unsigned int outputRefs[] = {10, 11};

  double inputs[] = {21.0};
  const unsigned int inputRefs[] = {10};

  while ( time < tEnd ) {
    result = getNextEventTime(&eventInfo, nullptr);
    std::cout << "Current time: " << time << std::endl;
    std::cout << "Next event time: " << eventInfo.nextEventTime << std::endl;

    result = setVariables(inputRefs, inputs, 1, nullptr);

    result = getVariables(outputRefs, outputs, 2, nullptr); 
    std::cout << "Output 10 - T: " << outputs[0] << std::endl;
    std::cout << "Output 11 - H: " << outputs[1] << std::endl;

    time = eventInfo.nextEventTime;
    setTime(time, nullptr);
  }

  terminate(nullptr);
};
