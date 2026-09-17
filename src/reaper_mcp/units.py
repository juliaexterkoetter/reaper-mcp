"""Human audio units. None represents negative-infinity dB (silence)."""

import math


def db_to_gain(db: float | None) -> float:
    if db is None:
        return 0.0
    if not math.isfinite(db) or not -150 <= db <= 24:
        raise ValueError("dB must be finite and between -150 and +24; null means silence")
    return 10 ** (db / 20)


def gain_to_db(gain: float) -> float | None:
    if not math.isfinite(gain) or gain < 0:
        raise ValueError("Gain must be finite and nonnegative")
    return 20 * math.log10(gain) if gain else None
