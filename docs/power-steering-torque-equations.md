# Power Steering Torque Equations

Equations and relationships for determining optimal torque settings for power steering systems based on vehicle speed and engine RPM.

## Basic Speed-Sensitive Assist Equation

The fundamental relationship is:

```
T_assist = T_max × G(v) × H(T_driver)
```

Where:
- `T_assist` = motor/pump assist torque
- `T_max` = maximum assist capability
- `G(v)` = speed-dependent gain (0 to 1)
- `H(T_driver)` = function of driver steering torque input

## Speed-Dependent Gain Functions

### Exponential Decay (Most Common)

```
G(v) = G_min + (1 - G_min) × e^(-v/v₀)
```

Parameters:
- `G_min` ≈ 0.2–0.4 (highway feel, minimum assist)
- `v₀` ≈ 30–50 km/h (characteristic speed)

### Sigmoid/Logistic

```
G(v) = G_min + (1 - G_min) / (1 + e^((v - v_mid)/k))
```

Provides sharper transition around `v_mid`.

### Linear Decay (Simple)

```
G(v) = max(G_min, 1 - k × v)
```

Where `k` is a decay constant.

## Hydraulic Power Steering (Engine RPM Dependent)

For traditional hydraulic systems, pump flow is proportional to engine RPM:

```
Q = D × RPM / 1000
```

Where:
- `Q` = flow rate (L/min)
- `D` = pump displacement (cc/rev)

Assist pressure relationship:

```
P = min(P_relief, k × T_required / Q)
```

At low RPM (idle), steering rate may need to be limited or idle RPM increased to maintain adequate flow.

## Electric Power Steering (EPS) Motor Control

### Torque Map Approach

Modern EPS systems use 2D or 3D lookup tables:

**Inputs:**
- Vehicle speed
- Driver torque (measured at steering column)
- Steering angle rate (for damping)

**Output:**
- Motor current/torque command

### Basic EPS Assist Equation

```
T_motor = K_p(v) × T_driver + K_d(v) × ω_steering
```

Where:
- `K_p(v)` = speed-dependent proportional gain
- `K_d(v)` = speed-dependent damping gain
- `ω_steering` = steering angular velocity

### Return-to-Center Torque

Additional torque to help return steering to center:

```
T_return = K_return(v) × θ_steering
```

Where `θ_steering` is the steering angle from center.

## Tuning Considerations

1. **Low speed (parking):** Maximum assist, light steering feel
2. **Medium speed (urban):** Moderate assist, balanced feel
3. **High speed (highway):** Minimum assist, direct and stable feel

The "optimal" values are typically tuned empirically through driver feel studies rather than derived purely from equations. OEMs use extensive calibration testing to determine final torque maps.

## References

- SAE J1715: Electric Power Steering Systems
- ISO 13674: Road vehicles - Test method for steering systems
