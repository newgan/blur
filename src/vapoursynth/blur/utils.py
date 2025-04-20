from pathlib import Path


class BlurException(Exception):
    pass


def safe_int(value):
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def coalesce(value, fallback):
    return fallback if value is None else value


def get_model_path(model_name: str):
    rife_model_path = Path(__file__).parent / f"../models/{model_name}"

    if not rife_model_path.exists():
        raise BlurException(f"RIFE model not found at {rife_model_path}")

    return rife_model_path
