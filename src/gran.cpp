#include "gran.hpp"
#include <cmath>

GranSim::GranSim(const Matrix& position, const Array& radii, 
        const Array& mass, double young_mod, double friction, double damp_normal,
        double damp_tangent, double dt): position(position),
        radii(radii), mass(mass), young_mod(young_mod), friction(friction),
        damp_normal(damp_normal), damp_tangent(damp_tangent), dt(dt) {

    time = 0;
    Nparticles = position.rows();

    velocity = Matrix::Zero(Nparticles,2);
    rd2 = Matrix::Zero(Nparticles,2);
    rd3 = Matrix::Zero(Nparticles,2);
    rd4 = Matrix::Zero(Nparticles,2);
    force = Matrix::Zero(Nparticles,2);
}

void GranSim::predict() {
	const double a1 = dt;
    const double a2 = a1*dt/2.0;
    const double a3 = a2*dt/3.0;
    const double a4 = a3*dt/4.0;

	position += a1*velocity + a2*rd2 + a3*rd3 + a4*rd4;
	velocity += a1*rd2 + a2*rd3 + a4*rd4;
	rd2 += a1*rd3 + a2*rd4;
	rd3 += a1*rd4;
}

void GranSim::correct() {
    const double c0 = 19.0/180.0*pow(dt,2);
    const double c1 = 3.0/8.0*dt;
    const double c3 = 3.0/2.0/dt;
    const double c4 = 1.0/pow(dt,2);

    for (int i=0; i<Nparticles; i++) {
        auto accel = force.row(i)/mass(i);
        auto corr = accel - rd2.row(i);
        position.row(i) += c0*corr;
        velocity.row(i) += c1*corr;
        rd2.row(i) = accel;
        rd2.row(i) += c3*corr;
        rd4.row(i) += c4*corr;
    }

    time += dt;
}

void GranSim::compute_force() {
    force = 0;

    for (int i=0; i<Nparticles; i++) {
        for (int j=i+1; j<Nparticles; j++) {
            vec2 dr = position.row(i) - position.row(j);
            bool condition = (dr.squaredNorm() < (radii(i) + radii(j))*(radii(i) + radii(j)));


            if (condition) {
                double dr_norm = dr.norm();
                double overlap = radii(i) + radii(j) - dr_norm;
                dr /= dr_norm;
                vec2 dt(-dr(1), dr(0));
                vec2 dv = velocity.row(i) - velocity.row(j);

                double dv_n = -dr.dot(dv);
                double dv_t = dt.dot(dv);
                double reff = radii(i)*radii(j)/(radii(i) + radii(j));
                double Fn_mag = std::max(0.0, sqrt(reff)*young_mod*sqrt(overlap)*(overlap + damp_normal*dv_n));
                double Ft_mag = std::min(friction*Fn_mag, damp_tangent*std::abs(dv_t));
                auto F = dr.array()*Fn_mag - dt.array()*sgn(dv_t)*Ft_mag;
                force.row(i) += F;
                force.row(j) -= F;
            }
        }
    }

    for (int i=0; i<Nparticles; i++) {
        force.row(i) += mass(i)*vec2(0, -9.8).array();

        double overlap = radii(i) - position(i,1);
        if (overlap > 0) {
            double dv_n = -velocity(i,1);
            double dv_t = velocity(i,0);
            double reff = radii(i);
            double Fn_mag = std::max(0.0, sqrt(reff)*young_mod*sqrt(overlap)*(overlap + damp_normal*dv_n));
            double Ft_mag = std::min(friction*Fn_mag, damp_tangent*std::abs(dv_t));
            force.row(i) += vec2(-sgn(dv_t)*Ft_mag, Fn_mag).array();
        }
    }
}

void GranSim::step() {
    predict();
    compute_force();
    correct();
}