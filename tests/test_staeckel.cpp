#include "potential_staeckel.h"
#include "actions_staeckel.h"
#include "orbit.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

const double integr_eps=1e-8;        // integration accuracy parameter
const double eps=1e-6;               // accuracy of comparison
const double axis_a=1.6, axis_c=1.0; // axes of perfect ellipsoid

// helper class to compute scatter in actions
class actionstat{
public:
    actions::Actions avg, disp;
    int N;
    actionstat() { avg.Jr=avg.Jz=avg.Jphi=0; disp=avg; N=0; }
    void add(const actions::Actions act) {
        avg.Jr  +=act.Jr;   disp.Jr  +=pow_2(act.Jr);
        avg.Jz  +=act.Jz;   disp.Jz  +=pow_2(act.Jz);
        avg.Jphi+=act.Jphi; disp.Jphi+=pow_2(act.Jphi);
        N++;
    }
    void finish() {
        avg.Jr/=N;
        avg.Jz/=N;
        avg.Jphi/=N;
        disp.Jr  =sqrt(std::max<double>(0, disp.Jr/N  -pow_2(avg.Jr)));
        disp.Jz  =sqrt(std::max<double>(0, disp.Jz/N  -pow_2(avg.Jz)));
        disp.Jphi=sqrt(std::max<double>(0, disp.Jphi/N-pow_2(avg.Jphi)));
    }
};

template<typename coordSysT>
bool test_oblate_staeckel(const potential::StaeckelOblatePerfectEllipsoid& potential,
    const coord::PosVelT<coordSysT>& initial_conditions,
    const double total_time, const double timestep, const bool output)
{
    std::vector<coord::PosVelT<coordSysT> > traj;
    orbit::integrate(potential, initial_conditions, total_time, timestep, traj, integr_eps);
    actions::ActionFinderAxisymmetricStaeckel afs(potential);
    actions::ActionFinderAxisymmetricFudgeJS aff(potential, -pow_2(axis_a), -pow_2(axis_c));
    actionstat stats, statf;
    bool ex_afs=false, ex_aff=false;
    std::ofstream strm;
    if(output) {
        std::ostringstream s;
        double x[6];
        initial_conditions.unpack_to(x);
        s<<coordSysT::name()<<"_"<<x[0]<<x[1]<<x[2]<<x[3]<<x[4]<<x[5];
        strm.open(s.str().c_str());
    }
    for(size_t i=0; i<traj.size(); i++) {
        try{
            stats.add(afs.actions(coord::toPosVelCyl(traj[i])));
        }
        catch(std::exception &e) {
            ex_afs=true;
//            std::cout << "Exception in Staeckel at i="<<i<<": "<<e.what()<<"\n";
        }
        actions::ActionAngles a;
        try{
            a=aff.actionAngles(coord::toPosVelCyl(traj[i]));
            statf.add(a);
            if(fabs(a.Jr-statf.avg.Jr/statf.N)>1e-4)
                std::cout << a.Jr <<"!="<<(statf.avg.Jr/statf.N)  <<"\n";
        }
        catch(std::exception &e) {
            ex_aff=true;
//            std::cout << "Exception in Fudge at i="<<i<<": "<<e.what()<<"\n";
        }
        if(output) {
            double xv[6];
            traj[i].unpack_to(xv);
            strm << i*timestep<<"   " <<xv[0]<<" "<<xv[1]<<" "<<xv[2]<<"  "<<
                xv[3]<<" "<<xv[4]<<" "<<xv[5]<<"  "<<
                a.thetar<<" "<<a.thetaz<<" "<<a.thetaphi<<"\n";
        }
    }
    stats.finish();
    statf.finish();
    std::cout << coordSysT::name() << ", Exact"
    ":  Jr="  <<stats.avg.Jr  <<" +- "<<stats.disp.Jr<<
    ",  Jz="  <<stats.avg.Jz  <<" +- "<<stats.disp.Jz<<
    ",  Jphi="<<stats.avg.Jphi<<" +- "<<stats.disp.Jphi<< (ex_afs ? ",  CAUGHT EXCEPTION\n":"\n");
    std::cout << coordSysT::name() << ", Fudge"
    ":  Jr="  <<statf.avg.Jr  <<" +- "<<statf.disp.Jr<<
    ",  Jz="  <<statf.avg.Jz  <<" +- "<<statf.disp.Jz<<
    ",  Jphi="<<statf.avg.Jphi<<" +- "<<statf.disp.Jphi<< (ex_aff ? ",  CAUGHT EXCEPTION\n":"\n");
    return stats.disp.Jr<eps && stats.disp.Jz<eps && stats.disp.Jphi<eps;
}

bool test_three_cs(const potential::StaeckelOblatePerfectEllipsoid& potential, const coord::PosVelCar& initcond, const char* title)
{
    const double total_time=100.;
    const double timestep=1./8;
    bool output=true;
    bool ok=true;
    std::cout << "   ===== "<<title<<" =====\n";
    ok &= test_oblate_staeckel(potential, coord::toPosVelCar(initcond), total_time, timestep, output);
    ok &= test_oblate_staeckel(potential, coord::toPosVelCyl(initcond), total_time, timestep, output);
    ok &= test_oblate_staeckel(potential, coord::toPosVelSph(initcond), total_time, timestep, output);
    return ok;
}

int main() {
    const potential::StaeckelOblatePerfectEllipsoid potential(1.0, axis_a, axis_c);
    bool allok=true;
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.3, 0.1, 0.1, 0.4, 0.1), "ordinary case");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.0, 0.0, 0, 0.2, 0.3486), "thin orbit (Jr~0)");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.0, 0.0, 0.0, 0, 0.4732), "thin orbit in x-z plane (Jr~0, Jphi=0)");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.0, 0.0, 0.0, 0, 0.4097), "tube orbit in x-z plane near separatrix");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.0, 0.0, 0.0, 0, 0.4096), "box orbit in x-z plane near separatrix");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.3, 0., 0.1, 0.4, 1e-3), "almost in-plane orbit (Jz~0)");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0.3, 0., 0.1, 0.4, 0), "exactly in-plane orbit (Jz=0)");
    allok &= test_three_cs(potential, coord::PosVelCar(1, 0, 0, 0, 0.296, 0), "almost circular in-plane orbit (Jz=0,Jr~0)");
    if(allok)
        std::cout << "ALL TESTS PASSED\n";
    return 0;
}