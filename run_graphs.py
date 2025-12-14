import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import warnings

# Configuration
warnings.simplefilter(action='ignore', category=FutureWarning)

BASE_DIR = Path("CSV")
DIR_CONSTRAINTS = Path("Constraints")
OUTPUT_DIR = Path("Graphs")
OUTPUT_DIR.mkdir(exist_ok=True)

SCALING_FACTORS = ["1.00", "0.90", "0.80", "0.70", "0.60", "0.50", "0.40", "0.30", "0.20", "0.10"]

VARIANTS_G1 = ["FALLS", "LS", "LS_s0_p0", "LS_s1_p1"]
VARIANTS_G2 = ["LS", "LS_s0_p0", "LS_s1_p1"]

DFG_SIZES = {
    "example": 7, "hal": 11, "horner_bezier_surf_dfg__12": 18, "arf": 28,
    "motion_vectors_dfg__7": 32, "ewf": 34, "feedback_points_dfg__7": 53,
    "write_bmp_header_dfg__7": 106, "interpolate_aux_dfg__12": 108,
    "matmul_dfg__3": 109, "smooth_color_z_triangle_dfg__31": 197,
    "invert_matrix_general_dfg__3": 333, "h2v2_smooth_downsample_dfg__6": 51,
    "collapse_pyr_dfg__113": 56, "idctcol_dfg__3": 114,
    "jpeg_fdct_islow_dfg__6": 134, "random1": 601, "random2": 607,
    "random3": 806, "random4": 906, "random5": 1208, "random6": 1812,
    "random7": 2006,
}

# Helpers
def clean_dfg_name(name):
    if not isinstance(name, str): return "unknown"
    name = name.replace(".txt", "")
    for s in ["_4type_invdelay", "_4type_uniform"]:
        name = name.replace(s, "")
    return name

def get_dfg_size(name):
    return DFG_SIZES.get(clean_dfg_name(name), np.nan)

def identify_variant(filename):
    fname = filename.upper()
    if "RESULTS_FDS" in fname: return "FDS"
    if "_S1_P1" in fname: return "LS_s1_p1"
    if "_S0_P0" in fname: return "LS_s0_p0"
    if "RESULTS_LS_" in fname and "_S" not in fname and "_P" not in fname: return "LS"
    return "other"

def load_results_from_folder(folder_path):
    if not folder_path.exists(): return pd.DataFrame()
    frames = []
    for file_path in folder_path.glob("*.csv"):
        variant = identify_variant(file_path.name)
        if variant == "other": continue 
        try:
            df = pd.read_csv(file_path)
            cols_map = {}
            for c in df.columns:
                if "Actual_Latency" in c: cols_map[c] = "Latency"
                elif "Runtime" in c or "time" in c.lower(): cols_map[c] = "Runtime"
            df = df.rename(columns=cols_map)
            if "Latency" not in df.columns: continue
            if "Runtime" not in df.columns: df["Runtime"] = np.nan
            df["Variant"] = variant
            df["Mode"] = "uniform" if "uniform" in file_path.name else "invdelay"
            df["Factor"] = folder_path.name
            df["DFG_Clean"] = df["DFG_Name"].apply(clean_dfg_name)
            df["DFG_Size"] = df["DFG_Name"].apply(get_dfg_size)
            frames.append(df[["DFG_Clean", "DFG_Size", "Latency", "Runtime", "Variant", "Mode", "Factor"]])
        except: pass
    return pd.concat(frames, ignore_index=True) if frames else pd.DataFrame()


def load_baseline_constraints(current_factor):
    rows = []
    for txt_file in DIR_CONSTRAINTS.glob("*.txt"):
        mode = "uniform" if "uniform" in txt_file.name else "invdelay"
        try:
            with open(txt_file, "r") as f:
                for line in f:
                    if line.startswith("//") or not line.strip(): continue
                    parts = line.split()
                    rows.append({
                        "DFG_Clean": parts[0], 
                        "Latency": int(parts[1]),
                        "Runtime": 0,
                        "Variant": "FALLS",
                        "Mode": mode,
                        "Factor": current_factor,
                        "DFG_Size": get_dfg_size(parts[0])
                    })
        except: pass
    return pd.DataFrame(rows)

# Plotting functions

def plot_graph_1_delta(df, mode, factor):
    subset = df[(df["Mode"] == mode) & (df["Factor"] == factor)].copy()
    if subset.empty: return
    
    piv = subset.pivot_table(index="DFG_Clean", columns="Variant", values="Latency")
    if "FALLS" not in piv.columns: return
    
    for col in piv.columns:
        if col != "FALLS": piv[col] = piv[col] - piv["FALLS"]
    piv["FALLS"] = 0 
    
    piv["size"] = piv.index.map(DFG_SIZES)
    piv = piv.sort_values("size").drop(columns="size")

    plt.figure(figsize=(12, 6))
    styles = {
        "FALLS": {"c": "black", "ls": "--", "m": None},
        "FDS":   {"c": "red",   "ls": "-",  "m": "x"},
        "LS":    {"c": "blue",  "ls": "-",  "m": "o"},
        "LS_s0_p0": {"c": "orange", "ls": "-", "m": "^"},
        "LS_s1_p1": {"c": "green",  "ls": "-", "m": "s"}
    }
    
    for var in VARIANTS_G1:
        if var in piv.columns:
            s = styles.get(var, {"c": "gray", "ls": "-", "m": "."})
            plt.plot(piv.index, piv[var], color=s["c"], linestyle=s["ls"], marker=s["m"], label=var, alpha=0.8)

    plt.xticks(rotation=45, ha="right", fontsize=9)
    plt.ylabel("Latency Delta vs FALLS (Lower is Better)")
    plt.title(f"Graph 1: Latency Delta ({mode}) - Factor {factor}")
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.4)
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / f"Graph1_Delta_{mode}_{factor}.png")
    plt.close()
    print(f"\tSaved: Graph1_Delta_{mode}_{factor}.png")

def plot_graph_2_runtime(df, mode):
    subset = df[(df["Mode"] == mode) & (df["Factor"] == "1.00") & (df["Variant"].isin(VARIANTS_G2))].copy()
    if subset.empty: return
    
    plt.figure(figsize=(10, 6))
    for var in VARIANTS_G2:
        data = subset[subset["Variant"] == var].sort_values("DFG_Size")
        if data.empty: continue
        plt.scatter(data["DFG_Size"], data["Runtime"], alpha=0.7, label=var)
        try:
            valid = data.dropna(subset=["DFG_Size", "Runtime"])
            if len(valid) > 2:
                z = np.polyfit(valid["DFG_Size"], valid["Runtime"], 1)
                p = np.poly1d(z)
                plt.plot(valid["DFG_Size"], p(valid["DFG_Size"]), linestyle='--', alpha=0.5)
        except: pass
        
    plt.xlabel("DFG Size")
    plt.ylabel("Runtime (ms)")
    plt.title(f"Graph 2: Runtime ({mode}) - Factor 1.00")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / f"Graph2_Runtime_{mode}_1.00.png")
    plt.close()
    print(f"\tSaved: Graph2_Runtime_{mode}_1.00.png")

def plot_graph_3_improvement(df, mode):
    subset = df[(df["Mode"] == mode) & (df["Factor"] == "1.00")].copy()
    piv = subset.pivot_table(index="DFG_Clean", columns="Variant", values="Latency")
    if "LS" not in piv.columns: return
    
    results = {}
    for var in ["LS_s0_p0", "LS_s1_p1"]:
        if var in piv.columns:
            imp = (piv["LS"] - piv[var]) / piv["LS"] * 100
            results[var] = imp.mean()
            
    if not results: return
    plt.figure(figsize=(6, 5))
    bars = plt.bar(results.keys(), results.values(), color=['skyblue', 'green'])
    plt.axhline(0, color='black')
    plt.ylabel("% Improvement (Positive = Better)")
    plt.title(f"Graph 3: Improvement ({mode}) - Factor 1.00")
    for bar in bars:
        h = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2, h, f"{h:.2f}%", ha='center', va='bottom' if h>0 else 'top')
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / f"Graph3_Improvement_{mode}_1.00.png")
    plt.close()
    print(f"\tSaved: Graph3_Improvement_{mode}_1.00.png")

def plot_graph_4_global_weighted(df_all, mode):
    # Calculate global impact comparing both s0_p0 and s1_p1 against Standard LS
    subset = df_all[df_all["Mode"] == mode].copy()
    if subset.empty: return

    results = []
    targets = ["LS_s0_p0", "LS_s1_p1"]

    for factor in SCALING_FACTORS:
        sub_f = subset[subset["Factor"] == factor]
        if sub_f.empty: continue
        
        piv = sub_f.pivot_table(index="DFG_Clean", columns="Variant", values="Latency")
        
        # Calculate for both variants
        for var in targets:
            if "LS" in piv.columns and var in piv.columns:
                valid_pairs = piv.dropna(subset=["LS", var])
                if not valid_pairs.empty:
                    sum_ls = valid_pairs["LS"].sum()
                    sum_var = valid_pairs[var].sum()
                    
                    # Global % change: (Var - LS) / LS
                    global_change = (sum_var - sum_ls) / sum_ls * 100
                    
                    results.append({
                        "Factor": float(factor), 
                        "Variant": var, 
                        "Avg_Change": global_change
                    })

    if not results: return
    res_df = pd.DataFrame(results).sort_values("Factor", ascending=False)

    plt.figure(figsize=(10, 6))
    
    # Auto-zoom calculation based on ALL data points
    d_min = res_df["Avg_Change"].min()
    d_max = res_df["Avg_Change"].max()
    
    if d_min == d_max == 0:
        margin = 1.0
    else:
        span = abs(d_max - d_min)
        margin = max(span * 0.15, 0.05)
        
    ylim_min = d_min - margin
    ylim_max = d_max + margin
    
    # Zones
    plt.axhspan(0, ylim_max + 100, color='red', alpha=0.1)
    plt.axhspan(ylim_min - 100, 0, color='green', alpha=0.1)
    
    plt.text(0.15, ylim_max - (margin*0.5), "WORSE (Lat. Increase)", color='darkred', fontweight='bold', ha='right', fontsize=8)
    plt.text(0.15, ylim_min + (margin*0.5), "BETTER (Lat. Decrease)", color='darkgreen', fontweight='bold', ha='right', fontsize=8)

    # Plot Lines
    styles = {
        "LS_s0_p0": {"c": "orange", "m": "^", "lbl": "s0_p0 (No Feat)"},
        "LS_s1_p1": {"c": "black",  "m": "o", "lbl": "s1_p1 (Opt)"}
    }

    for var in targets:
        data = res_df[res_df["Variant"] == var]
        if not data.empty:
            s = styles.get(var)
            plt.plot(data["Factor"], data["Avg_Change"], 
                     marker=s["m"], color=s["c"], linewidth=2, label=s["lbl"])
            
            # Add labels
            for _, row in data.iterrows():
                val = row['Avg_Change']
                # Offset labels slightly to avoid overlap if points are close
                offset = margin * 0.3 if val >= 0 else - (margin * 0.3)
                if var == "LS_s0_p0": offset *= 1.5 # Push orange labels further out
                
                plt.text(row["Factor"], val + offset, f"{val:.3f}%", 
                         ha='center', fontsize=8, fontweight='bold', color=s["c"])

    plt.axhline(0, color='black', linewidth=1, linestyle='-')
    plt.gca().invert_xaxis()
    plt.ylim(ylim_min, ylim_max)
    plt.xlabel("Constraint Factor (Stricter --->)")
    plt.ylabel("% Global Latency Change vs LS")
    plt.title(f"Graph 4: Global Impact Analysis ({mode})")
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.5)
    plt.tight_layout()
    
    filename = OUTPUT_DIR / f"Graph4_Scaling_Delta_{mode}.png"
    plt.savefig(filename)
    plt.close()
    print(f"\tSaved: {filename.name}")
    
def main():
    
    all_factors_data = []

    for factor in SCALING_FACTORS:
        folder = BASE_DIR / factor
        if not folder.exists(): 
            print(f"Skipping {factor} (Folder not found)")
            continue
            
        print(f"Processing Factor: {factor}")
        
        df_csv = load_results_from_folder(folder)
        df_falls = load_baseline_constraints(factor)
        df_factor = pd.concat([df_csv, df_falls], ignore_index=True)
        
        if not df_factor.empty:
            for mode in ["uniform", "invdelay"]:
                plot_graph_1_delta(df_factor, mode, factor)
            
            if factor == "1.00":
                for mode in ["uniform", "invdelay"]:
                    plot_graph_2_runtime(df_factor, mode)
                    plot_graph_3_improvement(df_factor, mode)
            
            all_factors_data.append(df_csv)

    if all_factors_data:
        full_df = pd.concat(all_factors_data, ignore_index=True)
        print("\nGenerating Graph 4")
        for mode in ["uniform", "invdelay"]:
            plot_graph_4_global_weighted(full_df, mode)

    print("\nDone. Exiting.")

if __name__ == "__main__":
    main()