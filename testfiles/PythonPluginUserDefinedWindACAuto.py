import sys
import math

from pyenergyplus.plugin import EnergyPlusPlugin


class QuadraticCurve(object):
    def __init__(self, c1=None, c2=None, c3=None, min_x=None, max_x=None):
        self.c1 = c1
        self.c2 = c2
        self.c3 = c3
        if max_x is None:
            self.max_x = sys.float_info.max
        else:
            self.max_x = max_x

        if min_x is None:
            self.min_x = -sys.float_info.max
        else:
            self.min_x = min_x

    def curve_value(self, x):
        x = max(min(x, self.max_x), self.min_x)
        return self.c1 + (self.c2 * x) + (self.c3 * pow(x, 2))


class BiQuadraticCurve(object):
    def __init__(self, c1=None, c2=None, c3=None, c4=None, c5=None, c6=None,
                 min_x=None, max_x=None, min_y=None, max_y=None):
        self.c1 = c1
        self.c2 = c2
        self.c3 = c3
        self.c4 = c4
        self.c5 = c5
        self.c6 = c6
        if max_x is None:
            self.max_x = sys.float_info.max
        else:
            self.max_x = max_x

        if min_x is None:
            self.min_x = -sys.float_info.max
        else:
            self.min_x = min_x

        if max_y is None:
            self.max_y = sys.float_info.max
        else:
            self.max_y = max_y

        if min_y is None:
            self.min_y = -sys.float_info.max
        else:
            self.min_y = min_y

    def curve_value(self, x, y):
        x = max(min(x, self.max_x), self.min_x)
        y = max(min(y, self.max_y), self.min_y)

        return self.c1 + (self.c2 * x) + (self.c3 * pow(x, 2)) + (self.c4 * y) + (self.c5 * pow(y, 2)) + (
                self.c6 * x * y)


class Zone1WinACModel(EnergyPlusPlugin):

    def __init__(self):
        # init parent class
        super().__init__()

        # members
        self.Qdot_Request = 0.0
        self.PrimAir_MdotIn = 0.0
        self.PrimAir_MdotOut = 0.0
        self.PrimAir_Tinlet = 0.0
        self.PrimAir_Winlet = 0.0
        self.PrimAir_Tout = 0.0
        self.PrimAir_Wout = 0.0
        self.tot_cool_Power = 0.0
        self.ElectPower = 0.0
        self.ElectEnergy = 0.0
        self.OA_MdotIn = 0.0
        self.AirOAMdotDesign = 0.0
        self.AirSupMdotDesign = 0.0
        self.OA_Tdb = 0.0
        self.OA_W = 0.0
        self.FanEff = 0.0
        self.FanDeltaP = 0.0
        self.RatedCap = 0.0
        self.ZoneCoolTstat = 0.0
        self.RatedEIR = 0.0

        # handles
        self.need_to_get_handles = True
        self.Zone1WinAC_PrimAir_Tinlet_handle = None
        self.Zone1WinAC_PrimAir_Winlet_handle = None
        self.Zone1WinAC_OA_Tdb_handle = None
        self.Zone1WinAC_OA_W_handle = None
        self.Zone1WinAC_Qdot_Request_handle = None
        self.Zone1_OADesign_Vdot_handle = None
        self.Zone1WinAC_OA_rho_handle = None
        self.Zone1_CoolDesign_Mdot_handle = None
        self.Zone1WinAC_PrimAir_Cp_handle = None
        self.Zone1_CoolDesign_Cap_handle = None
        self.Zone1Cooling_Tstat_handle = None
        self.COOLINGCOILAVAILSCHED_handle = None

        # psych api instance
        self.psych = self.api.functional.psychrometrics()

        # curves
        d_capFT = {"c1": 0.942587793,  # Constant
                   "c2": 0.00954334,  # x
                   "c3": 0.000683770,  # x**2
                   "c4": -0.011042676,  # y
                   "c5": 0.000005249,  # y**2
                   "c6": -0.000009720,  # x*y
                   "min_x": 12.77778,
                   "max_x": 23.88889,
                   "min_y": 18.0,
                   "max_y": 46.11111}

        self.WindACCoolCapFT = BiQuadraticCurve(**d_capFT)

        d_capFFF = {"c1": 0.8,
                    "c2": 0.2,
                    "c3": 0.0,
                    "min_x": 0.5,
                    "max_x": 1.5}

        self.WindACCoolCapFFF = QuadraticCurve(**d_capFFF)

        d_PLFFPLR = {"c1": 0.85,
                     "c2": 0.15,
                     "c3": 0.0,
                     "min_x": 0.0,
                     "max_x": 1.0}

        self.WindACPLFFPLR = QuadraticCurve(**d_PLFFPLR)

        d_EIRFT = {"c1": 0.342414409,  # Constant
                   "c2": 0.034885008,  # x
                   "c3": -0.000623700,  # x**2
                   "c4": 0.004977216,  # y
                   "c5": 0.000437951,  # y**2
                   "c6": -0.000728028,  # x*y
                   "min_x": 12.77778,
                   "max_x": 23.88889,
                   "min_y": 18.0,
                   "max_y": 46.11111}

        self.WindACEIRFT = BiQuadraticCurve(**d_EIRFT)

        d_EIRFFF = {"c1": 1.1552,
                    "c2": -0.1808,
                    "c3": 0.0256,
                    "min_x": 0.5,
                    "max_x": 1.0}

        self.WindACEIRFFF = QuadraticCurve(**d_EIRFFF)

    def get_handles(self):
        self.Zone1WinAC_PrimAir_Tinlet_handle = self.api.exchange.get_internal_variable_handle(
            "Inlet Temperature for Primary Air Connection",
            "Zone1WindAC"
        )
        self.Zone1WinAC_PrimAir_Winlet_handle = self.api.exchange.get_internal_variable_handle(
            "Inlet Humidity Ratio for Primary Air Connection",
            "Zone1WindAC"
        )
        self.Zone1WinAC_OA_Tdb_handle = self.api.exchange.get_internal_variable_handle(
            "Inlet Temperature for Secondary Air Connection",
            "Zone1WindAC"
        )
        self.Zone1WinAC_OA_W_handle = self.api.exchange.get_internal_variable_handle(
            "Inlet Humidity Ratio for Secondary Air Connection",
            "Zone1WindAC"
        )
        self.Zone1WinAC_Qdot_Request_handle = self.api.exchange.get_internal_variable_handle(
            "Remaining Sensible Load to Cooling Setpoint",
            "Zone1WindAC"
        )
        self.Zone1_OADesign_Vdot_handle = self.api.exchange.get_internal_variable_handle(
            "Zone Outdoor Air Design Volume Flow Rate",
            "West Zone"
        )
        self.Zone1WinAC_OA_rho_handle = self.api.exchange.get_internal_variable_handle(
            "Inlet Density for Secondary Air Connection",
            "Zone1WindAC"
        )
        self.Zone1_CoolDesign_Mdot_handle = self.api.exchange.get_internal_variable_handle(
            "Final Zone Design Cooling Air Mass Flow Rate",
            "West Zone"
        )
        self.Zone1WinAC_PrimAir_Cp_handle = self.api.exchange.get_internal_variable_handle(
            "Inlet Specific Heat for Primary Air Connection",
            "Zone1WindAC"
        )
        self.Zone1_CoolDesign_Cap_handle = self.api.exchange.get_internal_variable_handle(
            "Final Zone Design Cooling Load",
            "West Zone"
        )
        self.Zone1Cooling_Tstat_handle = self.api.exchange.get_variable_handle(
            "Zone Thermostat Cooling Setpoint Temperature",
            "West Zone"
        )
        self.COOLINGCOILAVAILSCHED_handle = self.api.exchange.get_variable_handle(
            "Schedule Value",
            "COOLINGCOILAVAILSCHED"
        )
        self.OA_Press_handle = self.api.exchange.get_variable_handle(
            "Site Outdoor Air Barometric Pressure",
            "Environment"
        )
        self.Zone1WinAC_PrimAir_MdotOut_handle = self.api.exchange.get_actuator_handle(
            "Primary Air Connection",
            "Outlet Mass Flow Rate",
            "Zone1WindAC"
        )
        self.Zone1WinAC_PrimAir_MdotIn_handle = self.api.exchange.get_actuator_handle(
            "Primary Air Connection",
            "Inlet Mass Flow Rate",
            "Zone1WindAC"
        )
        self.Zone1WinAC_PrimAir_Tout_handle = self.api.exchange.get_actuator_handle(
            "Primary Air Connection",
            "Outlet Temperature",
            "Zone1WindAC"
        )
        self.Zone1WinAC_PrimAir_Wout_handle = self.api.exchange.get_actuator_handle(
            "Primary Air Connection",
            "Outlet Humidity Ratio",
            "Zone1WindAC"
        )
        self.Zone1WinAC_ElectPower_handle = self.api.exchange.get_global_handle(
            "Zone1WinAC_ElectPower"
        )
        self.Zone1WinAC_tot_cool_Power_handle = self.api.exchange.get_global_handle(
            "Zone1WinAC_tot_cool_Power"
        )
        self.Zone1WinAC_ElectEnergy_handle = self.api.exchange.get_global_handle(
            "Zone1WinAC_ElectEnergy"
        )
        self.Zone1WinAC_OA_MdotIn_handle = self.api.exchange.get_actuator_handle(
            "Secondary Air Connection",
            "Inlet Mass Flow Rate",
            "Zone1WindAC"
        )
        self.need_to_get_handles = False

    def initialize(self):
        self.PrimAir_Tinlet = self.api.exchange.get_internal_variable_value(self.Zone1WinAC_PrimAir_Tinlet_handle)
        self.PrimAir_Winlet = self.api.exchange.get_internal_variable_value(self.Zone1WinAC_PrimAir_Winlet_handle)
        self.OA_Tdb = self.api.exchange.get_internal_variable_value(self.Zone1WinAC_OA_Tdb_handle)
        self.OA_W = self.api.exchange.get_internal_variable_value(self.Zone1WinAC_OA_W_handle)
        self.Qdot_Request = self.api.exchange.get_internal_variable_value(self.Zone1WinAC_Qdot_Request_handle)
        Zone1_OADesign_Vdot = self.api.exchange.get_internal_variable_value(self.Zone1_OADesign_Vdot_handle)
        Zone1WinAC_OA_rho = self.api.exchange.get_internal_variable_value(self.Zone1WinAC_OA_rho_handle)
        self.AirOAMdotDesign = Zone1_OADesign_Vdot * Zone1WinAC_OA_rho
        self.AirSupMdotDesign = self.api.exchange.get_internal_variable_value(self.Zone1_CoolDesign_Mdot_handle)
        self.FanEff = 0.5
        self.FanDeltaP = 75.0
        self.RatedCap = self.api.exchange.get_internal_variable_value(self.Zone1_CoolDesign_Cap_handle) / 0.75
        self.RatedEIR = 1.0 / 3.0
        self.ZoneCoolTstat = self.api.exchange.get_variable_value(self.Zone1Cooling_Tstat_handle)

    def simulate(self):
        # sim block
        if self.Qdot_Request < 0.0:
            QdotReq = self.Qdot_Request
        else:
            QdotReq = 0.0

        if self.api.exchange.get_variable_value(self.COOLINGCOILAVAILSCHED_handle) == 0.0:
            QdotReq = 0.0

        if QdotReq == 0.0:
            self.PrimAir_MdotIn = 0.0
            self.PrimAir_MdotOut = 0.0
            self.PrimAir_Tout = self.PrimAir_Tinlet
            self.PrimAir_Wout = self.PrimAir_Winlet
            self.tot_cool_Power = 0.0
            self.ElectPower = 0.0
            self.ElectEnergy = 0.0
            self.OA_MdotIn = 0.0
            return

        Recirc_Mdot = self.AirSupMdotDesign - self.AirOAMdotDesign
        Recirc_H = self.psych.enthalpy(self.PrimAir_Tinlet, self.PrimAir_Winlet)
        OA_H = self.psych.enthalpy(self.OA_Tdb, self.OA_W)
        Mix_H = ((Recirc_Mdot * Recirc_H) + (self.AirOAMdotDesign * OA_H)) / self.AirSupMdotDesign
        Mix_W = ((Recirc_Mdot * self.PrimAir_Winlet) + (self.AirOAMdotDesign * self.OA_W)) / self.AirSupMdotDesign
        Mix_Tdb = self.psych.dry_bulb(Mix_H, Mix_W)
        OA_Press = self.api.exchange.get_variable_value(self.OA_Press_handle)
        Mix_rho = self.psych.density(OA_Press, Mix_Tdb, Mix_W)
        FanPower = (self.AirSupMdotDesign * self.FanDeltaP) / (self.FanEff * Mix_rho)
        FanPowerToAir = FanPower
        FanDeltaH = FanPowerToAir / self.AirSupMdotDesign
        FanOut_H = Mix_H + FanDeltaH
        FanOut_W = Mix_W
        FanOut_Tdb = self.psych.dry_bulb(FanOut_H, FanOut_W)
        FanOut_Twb = self.psych.wet_bulb(FanOut_Tdb, FanOut_W, OA_Press)
        RatedCBF = 0.12666
        LogRatedCBF = math.log(RatedCBF)
        A0 = (0.0 - 1.0) * LogRatedCBF * self.AirSupMdotDesign
        ADiff = (0.0 - 1.0) * (A0 / self.AirSupMdotDesign)
        CBF = math.exp(ADiff)
        TotCapTempModFac = self.WindACCoolCapFT.curve_value(FanOut_Twb, self.OA_Tdb)
        TotCapFlowModFac = self.WindACCoolCapFFF.curve_value(1.0)
        TotCap = self.RatedCap * TotCapTempModFac * TotCapFlowModFac
        HDelta = TotCap / self.AirSupMdotDesign
        H_ADP = FanOut_H - (HDelta / (1.0 - CBF))
        T_ADP = self.psych.saturation_temperature(H_ADP, OA_Press)
        W_ADP = self.psych.humidity_ratio(T_ADP, H_ADP)
        H_Tin_W_ADP = self.psych.enthalpy(FanOut_Tdb, W_ADP)
        if (FanOut_H - H_ADP) != 0.0:
            SHR = (H_Tin_W_ADP - H_ADP) / (FanOut_H - H_ADP)
            SHR = min(SHR, 1.0)
        else:
            SHR = 1.0
        PLR = 1.0
        FullLoadOutAirEnth = FanOut_H - (TotCap / self.AirSupMdotDesign)
        hTinwout = FanOut_H - ((1.0 - SHR) * HDelta)
        if SHR != 1.0:
            FullLoadOutAirHumRat = self.psych.humidity_ratio(FanOut_Tdb, hTinwout)
        else:
            FullLoadOutAirHumRat = FanOut_W
        FullLoadOutAirTemp = self.psych.dry_bulb(FullLoadOutAirEnth, FullLoadOutAirHumRat)
        DesiredZone_H = self.psych.enthalpy(self.ZoneCoolTstat, self.PrimAir_Winlet)
        FullTotCapSens = self.AirSupMdotDesign * (DesiredZone_H - FullLoadOutAirEnth)
        ABS_FullTotCapSens = abs(FullTotCapSens)
        ABS_QdotReq = abs(QdotReq)
        OutletAirTemp  = 0.0
        OutAir_W = 0.0
        if ABS_QdotReq < ABS_FullTotCapSens:
            PLR = (ABS_QdotReq - FanPowerToAir) / (ABS_FullTotCapSens - FanPowerToAir)
            ErrorTol = 0.0005
            Iter = 0
            MaxIter = 30
            Relax = 0.8
            ABS_Error = 0.002
            while (ABS_Error > ErrorTol) and (Iter < MaxIter):
                OutAir_H = (PLR * FullLoadOutAirEnth) + ((1.0 - PLR) * FanOut_H)
                OutAir_W = (PLR * FullLoadOutAirHumRat) + ((1.0 - PLR) * FanOut_W)
                OutletAirTemp = self.psych.dry_bulb(OutAir_H, OutAir_W)
                TotCapTest = self.AirSupMdotDesign * (DesiredZone_H - OutAir_H)
                Error = (ABS_QdotReq - TotCapTest) / ABS_QdotReq
                ABS_Error = abs(Error)
                if ABS_Error > ErrorTol:
                    DelPLR = (ABS_QdotReq - TotCapTest) / ABS_FullTotCapSens
                    PLR = PLR + Relax * DelPLR
                    PLR = min(PLR, 1.0)
                Iter = Iter + 1
                if Iter == 16:
                    Relax = 0.5
        else:
            PLR = 1.0
            OutAir_H = FullLoadOutAirEnth
            OutAir_W = FullLoadOutAirHumRat
            OutletAirTemp = FullLoadOutAirTemp

        PLF = self.WindACPLFFPLR.curve_value(PLR)
        CoilRTF = PLR / PLF
        CoilRTF = min(CoilRTF, 1.0)
        EIRTempModFac = self.WindACEIRFT.curve_value(FanOut_Twb, self.OA_Tdb)
        EIRFlowModFac = self.WindACEIRFFF.curve_value(1.0)
        EIR = self.RatedEIR * EIRTempModFac * EIRFlowModFac
        DXElecPower = TotCap * CoilRTF * EIR
        self.ElectPower = DXElecPower + FanPower
        self.ElectEnergy = self.ElectPower * self.api.exchange.system_time_step() * 3600
        self.tot_cool_Power = ABS_FullTotCapSens * PLR
        self.PrimAir_MdotOut = self.AirSupMdotDesign
        self.PrimAir_MdotIn = self.AirSupMdotDesign
        self.PrimAir_Tout = OutletAirTemp
        self.PrimAir_Wout = OutAir_W
        self.OA_MdotIn = self.AirOAMdotDesign

    def report(self):
        self.api.exchange.set_actuator_value(self.Zone1WinAC_PrimAir_MdotOut_handle, self.PrimAir_MdotOut)
        self.api.exchange.set_actuator_value(self.Zone1WinAC_PrimAir_MdotIn_handle, self.PrimAir_MdotIn)
        self.api.exchange.set_actuator_value(self.Zone1WinAC_PrimAir_Tout_handle, self.PrimAir_Tout)
        self.api.exchange.set_actuator_value(self.Zone1WinAC_PrimAir_Wout_handle, self.PrimAir_Wout)
        self.api.exchange.set_global_value(self.Zone1WinAC_ElectPower_handle, self.ElectPower)
        self.api.exchange.set_global_value(self.Zone1WinAC_tot_cool_Power_handle, self.tot_cool_Power)
        self.api.exchange.set_global_value(self.Zone1WinAC_ElectEnergy_handle, self.ElectEnergy)
        self.api.exchange.set_actuator_value(self.Zone1WinAC_OA_MdotIn_handle, self.OA_MdotIn)

    def on_user_defined_component_model(self) -> int:

        if self.api.exchange.api_data_fully_ready():
            if self.need_to_get_handles:
                self.get_handles()
            self.initialize()
            self.simulate()
            self.report()
            return 0
        else:
            return 0
