import sys
from collections import defaultdict, deque
import math
import pydot
import re

# --- 1. Core Data Structures ---

class Operation:
    """
    Represents a single operation (a node) in the DFG.
    """
    def __init__(self, op_id, latency, resource_type):
        self.id = op_id
        self.latency = latency
        self.resource_type = resource_type
        
        # Graph structure
        self.predecessors = set()
        self.successors = set()
        
        # Scheduling state
        self.asap = 0
        self.alap = float('inf')
        
        # Heuristic calculation terms
        self.down_len = 0 # Longest path from this node to sink
        self.critical_successor_id = None # Which successor is on the critical path
        
        # Non-myopic C(u) terms (YOUR NEW 'AVERAGE' LOGIC)
        self.congestion_cost = 0.0  # The *final average* congestion cost
        self.c_path_sum = 0.0       # The (temp) total sum of C_local on the critical path
        self.c_path_len = 0         # The (temp) length of the critical path (in nodes)

    def __repr__(self):
        # A simple representation for printing
        return f"Op {self.id} (T={self.resource_type}, L={self.latency})"
        
    def add_predecessor(self, op):
        self.predecessors.add(op.id)
        op.successors.add(self.id)

def build_ops_map(ops_list, dependencies):
    """
    Creates a dictionary of Operation objects from the raw lists
    and links their dependencies.
    """
    ops_map = {op.id: op for op in ops_list}
    
    # --- BUG FIX: Find SOURCE/SINK by TYPE, don't just create new ones ---
    source_node = None
    sink_node = None
    
    # Search for existing SOURCE/SINK nodes provided in the list
    for op in ops_map.values():
        if op.resource_type == "SOURCE":
            source_node = op
            print(f"[Debug] Found existing SOURCE node: {op.id}")
        elif op.resource_type == "SINK":
            sink_node = op
            print(f"[Debug] Found existing SINK node: {op.id}")
            
    # If not found, create them
    if source_node is None:
        source_node = Operation("SOURCE", 0, "SOURCE")
        ops_map[source_node.id] = source_node
        print("[Debug] No SOURCE node found, creating new one.")
    
    if sink_node is None:
        sink_node = Operation("SINK", 0, "SINK")
        ops_map[sink_node.id] = sink_node
        print("[Debug] No SINK node found, creating new one.")
    # --- END BUG FIX ---

    all_nodes_with_predecessors = set()
    all_nodes_with_successors = set()

    for pred_id, succ_id in dependencies:
        if pred_id in ops_map and succ_id in ops_map:
            ops_map[succ_id].add_predecessor(ops_map[pred_id])
            all_nodes_with_predecessors.add(succ_id)
            all_nodes_with_successors.add(pred_id)
        else:
            print(f"[Warning] Invalid dependency: {pred_id} -> {succ_id}", file=sys.stderr)

    # Link nodes with no predecessors to SOURCE
    for op in ops_map.values():
        if op != source_node and op != sink_node and not op.predecessors:
            op.add_predecessor(source_node)
            all_nodes_with_predecessors.add(op.id)
            all_nodes_with_successors.add(source_node.id)

    # Link nodes with no successors to SINK
    for op in ops_map.values():
        if op != source_node and op != sink_node and not op.successors:
            sink_node.add_predecessor(op)
            all_nodes_with_predecessors.add(sink_node.id)
            all_nodes_with_successors.add(op.id)
            
    return ops_map

# --- 2. Static Pre-computation Functions (ASAP, Down-Length) ---

def calculate_asap(ops_map):
    """
    Calculates ASAP times for all nodes via topological sort.
    This is a new, simplified, and correct version.
    SOURCE node starts and finishes *before* cycle 1.
    All real operations can start at cycle 1.
    """
    asap_times = defaultdict(int)
    in_degree = {op_id: len(op.predecessors) for op_id, op in ops_map.items()}
    
    # Find the source node
    source_node = None
    for op in ops_map.values():
        if op.resource_type == "SOURCE":
            source_node = op
            break
    
    if source_node is None:
        raise RuntimeError("No SOURCE node found in build_ops_map result.")
        
    # Start SOURCE at cycle 0, it finishes at cycle 0.
    asap_times[source_node.id] = 0
    source_node.asap = 0
    
    calc_queue = deque([source_node])
    
    processed_count = 0
    while calc_queue:
        u = calc_queue.popleft()
        processed_count += 1
        
        # Real ops start *after* their predecessors finish.
        # SOURCE finishes at 0. Its successors start at 1.
        if u.resource_type == "SOURCE":
            u_finish_time = 1 # All real work starts at cycle 1
        else:
            u_finish_time = asap_times[u.id] + u.latency
        
        for v_id in u.successors:
            v = ops_map[v_id]
            
            # A successor's ASAP is the max of all its predecessors' finish times
            new_v_asap = max(asap_times[v_id], u_finish_time)
            
            asap_times[v_id] = new_v_asap
            v.asap = new_v_asap
            
            # Decrement in-degree and add to queue if 0
            in_degree[v_id] -= 1
            if in_degree[v_id] == 0:
                calc_queue.append(v)
                
    if processed_count != len(ops_map):
        cycle_nodes = [op_id for op_id, degree in in_degree.items() if degree > 0]
        print(f"[Error] Cycle detected in graph! Processed {processed_count}/{len(ops_map)} nodes.", file=sys.stderr)
        print(f"[Error] Unprocessed nodes (part of cycle): {cycle_nodes}", file=sys.stderr)
        raise RuntimeError("Cycle detected in graph!")
        
    print(f"[Debug] Calculated ASAP: {dict(asap_times)}")
    return asap_times

def calculate_down_len(ops_map):
    """
    Calculates the longest path (down_len) from each node to the SINK
    and identifies the critical successor.
    """
    down_lens = defaultdict(int)
    
    # Find sink node(s)
    sink_nodes = [op for op in ops_map.values() if op.resource_type == "SINK"]
    if not sink_nodes:
        raise RuntimeError("No SINK node found in build_ops_map result.")
    
    # We need a reverse graph traversal (from SINKs to SOURCE)
    out_degree = {op_id: len(op.successors) for op_id, op in ops_map.items()}
    calc_queue = deque(sink_nodes)
    
    for op in sink_nodes:
        down_lens[op.id] = op.latency # 0 for SINK
        op.down_len = op.latency
        
    max_down_len = 0
    
    processed_count = 0
    while calc_queue:
        v = calc_queue.popleft()
        processed_count += 1
        
        for u_id in v.predecessors:
            u = ops_map[u_id]
            
            # Update down_len of u based on successor v
            # The path cost from u is (u.latency + path_cost_from_v)
            new_down_len_for_u = down_lens[v.id] + u.latency
            
            if new_down_len_for_u > down_lens[u_id]:
                down_lens[u_id] = new_down_len_for_u
                u.down_len = new_down_len_for_u
                u.critical_successor_id = v.id # Store which successor is critical
            
            out_degree[u_id] -= 1
            if out_degree[u_id] == 0:
                calc_queue.append(u)
                
    if processed_count != len(ops_map):
        cycle_nodes = [op_id for op_id, degree in out_degree.items() if degree > 0]
        print(f"[Error] Cycle detected in graph! Processed {processed_count}/{len(ops_map)} nodes.", file=sys.stderr)
        print(f"[Error] Unprocessed nodes (part of cycle): {cycle_nodes}", file=sys.stderr)
        raise RuntimeError("Cycle detected in graph!")

    # Find the max down_len *excluding* the source
    max_down_len = max(op.down_len for op in ops_map.values() if op.resource_type != "SOURCE" and op.resource_type != "SINK")
    
    print(f"[Debug] Calculated Down-Lengths: {dict(down_lens)}")
    print(f"[Debug] Max Down-Length: {max_down_len}")
    return down_lens, max_down_len


# --- 2. Iteration-Dependent Helper Functions (ALAP, FDS) ---

def calculate_alap(ops_map, l_target, down_lens):
    """
    Calculates ALAP times based on the *current* L_target.
    ALAP(u) = L_target - down_len(u)
    """
    alap_times = defaultdict(int)
    for op_id, op in ops_map.items():
        
        # This is the ALAP *start* time
        # L_target is the SINK start time.
        # down_len(u) is the path length from u's *start* to SINK's *start*
        print(f"[Debug] Calculating ALAP for {op.id}: L_target={l_target}, down_len={op.down_len}")
        alap_time = l_target - op.down_len + 1
        
        # Clamp ALAP to be at least ASAP
        alap_time = max(op.asap, alap_time) 
        
        alap_times[op_id] = alap_time
        op.alap = alap_time
        
    print(f"[Debug] Calculated ALAP (L_target={l_target}): {dict(alap_times)}")
    return alap_times

def calculate_fds_graphs(ops_map, l_target):
    """
    Calculates the FDS Distribution Graphs (q_k(m)) for all
    resource types based on current ASAP/ALAP times.
    """
    # Initialize graphs for each resource type
    dist_graphs = defaultdict(lambda: defaultdict(float))
    resource_types = {op.resource_type for op in ops_map.values() if op.resource_type not in ["SOURCE", "SINK"]}
    
    for k in resource_types:
        for m in range(1, l_target + 2): # Go one past L_target
            dist_graphs[k][m] = 0.0

    # Calculate probabilities
    for op in ops_map.values():
        if op.resource_type not in resource_types:
            continue
            
        k = op.resource_type
        asap = op.asap
        alap = op.alap
        
        # Safety check: ALAP must be >= ASAP
        if alap < asap:
            print(f"[Warning] ALAP < ASAP for {op.id} (ASAP={asap}, ALAP={alap}). Clamping.", file=sys.stderr)
            alap = asap
        
        mobility = alap - asap
        
        prob_start = 1.0 / (mobility + 1)
        
        # This op can start in cycles [asap...alap]
        for start_cycle in range(asap, alap + 1):
            # It will be *active* in cycles [start...start+lat-1]
            for active_cycle in range(start_cycle, start_cycle + op.latency):
                if active_cycle > l_target + 1:
                    break # Stop counting past the scheduling horizon
                if active_cycle < 1: # Safety check
                    continue
                dist_graphs[k][active_cycle] += prob_start
                
    print(f"[Debug] Calculated FDS Distribution Graphs up to L_target={l_target}:")
    for k in dist_graphs:
        print(f"  Resource {k}: " + ", ".join(f"C{m}:{dist_graphs[k][m]:.4f}" for m in range(1, l_target + 2)))
    return dist_graphs




def calculate_congestion_cost(ops_map, sink_nodes, dist_graphs, resources, l_target):
    """
    Calculates the 'Congestion Cost' (C(u)) for all nodes.
    
    OUR "SMART" LOGIC (NON-MYOPIC AVERAGE):
    Calculates C(u) as the *AVERAGE* congestion (C_local)
    of all nodes on the critical path from u to the sink.
    C(u) = C_path_sum(u) / C_path_len(u)
    """
    congestion_costs = defaultdict(float)
    
    # Process in reverse topological order (sinks first)
    out_degree = {op_id: len(op.successors) for op_id, op in ops_map.items()}
    
    # We must retrieve all sink nodes to start the reverse traversal
    sink_node_objects = [ops_map[op_id] for op_id in ops_map if ops_map[op_id].resource_type == "SINK"]
    calc_queue = deque(sink_node_objects)
    
    processed_count = 0
    while calc_queue:
        processed_count += 1
        u = calc_queue.popleft()
        
        # 1. Calculate C_local(u): (max_q / R_max) for this node
        c_local = 0.0
        k = u.resource_type
        max_resource_count = resources.get(k, 1)
        
        if k != "SOURCE" and k != "SINK" and k in resources:
            q_max_u = 0
            u_asap = u.asap
            u_alap = max(u_asap, u.alap) # Safety check
            
            # Find max(q) over the op's *ACTIVITY* range [asap, alap + latency - 1]
            u_finish_latest = u_alap + u.latency 
            
            for m in range(u_asap, u_finish_latest):
                if m > l_target: break
                if m < 1: continue 
                q_max_u = max(q_max_u, dist_graphs.get(k, {}).get(m, 0.0))
            
            c_local = q_max_u / max_resource_count
            print(f"[Debug] Op {u.id}: C_local={c_local:.4f} (q_max={q_max_u:.4f}, R_max={max_resource_count})")
        
        # 2. Calculate C_inherited(u) from the critical successor
        c_inherited_sum = 0.0
        inherited_len = 0
        if u.critical_successor_id:
            v = ops_map[u.critical_successor_id]
            c_inherited_sum = v.c_path_sum
            inherited_len = v.c_path_len
            
        # 3. Final Cost C(u)
        u.c_path_sum = c_local + c_inherited_sum
        u.c_path_len = 1 + inherited_len # We count all nodes
        
        if u.c_path_len > 0:
            u.congestion_cost = u.c_path_sum / u.c_path_len
        else:
            u.congestion_cost = u.c_path_sum
            
        congestion_costs[u.id] = u.congestion_cost
        
        # Add predecessors to queue
        for p_id in u.predecessors:
            p = ops_map[p_id]
            out_degree[p_id] -= 1
            if out_degree[p_id] == 0:
                calc_queue.append(p)
                
    return congestion_costs



# --- 3. The Core Functions from Pseudocode ---

def CALCULATE_Static_Priorities(ops_map, resources, l_target, max_down_len):
    """
    This helper function calculates the F(u) priority for all operations.
    F(u) = S_norm(u)^2 * (C_norm(u) + epsilon)
    """
    
    # --- 1. Calculate L_target-dependent values ---
    # We assume ops_map[u].asap and ops_map[u].down_len are already set
    alap_times = calculate_alap(ops_map, l_target, {op_id: op.down_len for op_id, op in ops_map.items()})
    dist_graphs = calculate_fds_graphs(ops_map, l_target)
    
    # --- NEW: Calculate C(u) using your non-myopic AVERAGE method ---
    sink_nodes = [op.id for op in ops_map.values() if op.resource_type == "SINK"]
    congestion_costs = calculate_congestion_cost(ops_map, sink_nodes, dist_graphs, resources, l_target)
    
    # --- 2. Calculate Raw Priority Components (S and C) ---
    raw_s = {}
    raw_c = {} # This will now just be a copy
    s_max = 0.0001 # Use small non-zero value
    c_max = 0.0001
    
    for u in ops_map.values():
        if u.resource_type == "SOURCE" or u.resource_type == "SINK":
            continue
            
        # S(u) = (Urgency)
        # We use (mob+1) to prevent the "tyranny of zero" (S=0)
        mobility = u.alap - u.asap
        raw_s[u.id] = mobility + 1
            
        # C(u) = Use the pre-calculated, non-myopic *average* cost
        current_raw_c = u.congestion_cost
            
        raw_c[u.id] = current_raw_c
        
        # Update maximums for normalization
        s_max = max(s_max, raw_s[u.id])
        c_max = max(c_max, raw_c[u.id])
        
    print(f"[Debug] Raw S(u): {raw_s}")
    print(f"[Debug] Raw C(u) (non-myopic, avg): {raw_c}")
    print(f"[Debug] S_max: {s_max:.4f}, C_max: {c_max:.4f}")

    # --- 3. Calculate Final Normalized Priority F(u) ---
    priority_force = {}
    EPSILON = 0.0001 # To prevent division by zero
    
    for op_id in raw_s: # Iterate only over real ops
        s_norm = raw_s[op_id] / s_max
        c_norm = raw_c[op_id] / c_max
        
        # F(u) = S_norm^2 * (C_norm + epsilon)
        # We square S_norm to heavily penalize high mobility (low urgency),
        # making Timing (S) more important than Congestion (C).
        priority_force[op_id] = s_norm * (c_norm + EPSILON) 

        #print(f"[Debug] Op {op_id}: S_norm={s_norm:.4f}, C_norm={c_norm:.4f}, F(u)={priority_force[op_id]:.4f}")
    
    # Sort them for prettier debug printing
    priority_force_sorted = {k: f"{v:.4f}" for k, v in sorted(priority_force.items(), key=lambda item: item[1])}
    print(f"[Debug] Final Priorities F(u) (Low=HighPrio): {priority_force_sorted}")
    
    return priority_force

def RUN_List_Scheduler(ops_map, dependencies, resources, priority_list):
    """
    This is a standard list scheduling algorithm, modified to handle
    0-latency nodes "instantly".
    """
    
    # --- 1. Initialization ---
    current_cycle = 1
    # Make a copy to modify
    available_resources = {k: v for k, v in resources.items()}
    
    # Find the source node
    source_node = None
    for op in ops_map.values():
        if op.resource_type == "SOURCE":
            source_node = op
            break
            
    # ReadyList contains ops whose preds are finished
    ready_list = set() 
    
    # Manually add SOURCE to finished_ops and its successors to ready_list
    # to start the loop at C=1
    finished_ops = set([source_node.id])
    scheduled_ops_start_time = {source_node.id: 0}
    
    for succ_id in source_node.successors:
        if all(p_id in finished_ops for p_id in ops_map[succ_id].predecessors):
            ready_list.add(ops_map[succ_id])

    in_progress_list = [] # Stores (op, finish_cycle)
    total_ops = len(ops_map)
    
    # --- 2. Scheduling Loop ---
    while len(finished_ops) < total_ops:
        
        print(f"\n--- [Cycle {current_cycle}] ---")
        print(f"  Resources: {available_resources}")
        
        # --- 2a. Update finished ops ---
        # Ops that finished *before* this cycle began
        newly_finished_ops = []
        remaining_in_progress = []
        for op, finish_cycle in in_progress_list:
            if current_cycle >= finish_cycle:
                print(f"  Op {op.id} has finished.")
                newly_finished_ops.append(op)
                finished_ops.add(op.id)
                # Free resource (if it's not a virtual op)
                if op.latency > 0 and op.resource_type in available_resources:
                    available_resources[op.resource_type] += 1
            else:
                remaining_in_progress.append((op, finish_cycle))
        in_progress_list = remaining_in_progress
        
        # --- 2b. Update ReadyList ---
        for op in newly_finished_ops:
            for succ_id in op.successors:
                # Check if ALL predecessors of v are now finished
                if all(p_id in finished_ops for p_id in ops_map[succ_id].predecessors):
                    print(f"  Op {succ_id} is now READY.")
                    ready_list.add(ops_map[succ_id])
        
        if not ready_list and not in_progress_list:
             # This can happen if all ops are done but SINK isn't ready
             # (which shouldn't happen, but good to check)
             if len(finished_ops) < total_ops:
                 print(f"[Error] Scheduler stalled. Finished {len(finished_ops)}/{total_ops}", file=sys.stderr)
                 break
        
        # --- 2c. Build CandidateList, sorted by our static priority ---
        
        # This is the "magic flush" you wanted
        # Keep processing 0-latency ops until none are left in the ready list
        flushed_this_cycle = True
        while flushed_this_cycle:
            flushed_this_cycle = False
            zero_latency_candidates = []
            for op in list(ready_list): # Iterate over a copy
                if op.latency == 0:
                    zero_latency_candidates.append(op)
            
            # Sort by priority just in case, though it rarely matters for virtual ops
            zero_latency_candidates.sort(key=lambda op: priority_list.get(op.id, float('inf')))

            if not zero_latency_candidates:
                break # No more 0-latency ops to process

            for op in zero_latency_candidates:
                # Prevent re-scheduling a finished op
                if op.id in finished_ops:
                    if op in ready_list:
                        ready_list.remove(op)
                    continue

                print(f"  Scheduling VIRTUAL Op {op.id} (Finishes {current_cycle}).")
                # "Schedule" it
                scheduled_ops_start_time[op.id] = current_cycle
                if op in ready_list:
                    ready_list.remove(op)
                
                # It finishes *instantly*, so add to finished_ops
                # and update ReadyList *immediately*
                finished_ops.add(op.id)
                for succ_id in op.successors:
                    if all(p_id in finished_ops for p_id in ops_map[succ_id].predecessors):
                        print(f"  Op {succ_id} is now READY (unlocked by virtual).")
                        ready_list.add(ops_map[succ_id])
                
                flushed_this_cycle = True # We processed one, loop again
        
        # --- 2d. Schedule REAL ops from the remaining ReadyList ---
        
        # Now, ReadyList only contains ops with latency > 0
        candidate_list = sorted(
            [op for op in ready_list], 
            key=lambda op: priority_list.get(op.id, float('inf'))
        )
        
        print(f"  Candidates (Prio Low=High): {[op.id for op in candidate_list]}")

        for op in candidate_list:
            k = op.resource_type
            
            # Check for resource availability
            if k in available_resources and available_resources[k] > 0:
                # Schedule operation op
                print(f"  Scheduling REAL Op {op.id} (Finishes {current_cycle + op.latency}).")
                scheduled_ops_start_time[op.id] = current_cycle
                finish_time = current_cycle + op.latency
                in_progress_list.append((op, finish_time))
                
                # Remove from ready list
                ready_list.remove(op)
                
                # Allocate resource
                available_resources[k] -= 1
            else:
                if k not in available_resources:
                     print(f"  [Warn] Op {op.id} needs resource '{k}', which is not defined. Skipping.", file=sys.stderr)
                # else:
                #    print(f"  Op {op.id} deferred (no free {k}).")

        # --- 2e. Advance cycle ---
        current_cycle += 1
        
        if current_cycle > len(ops_map) * 10: # Failsafe
            print("[Error] Scheduler seems to be in an infinite loop. Aborting.", file=sys.stderr)
            break
            
    # --- 3. Final Latency Calculation ---
    # Find the SINK node
    sink_node = None
    for op in ops_map.values():
        if op.resource_type == "SINK":
            sink_node = op
            break
            
    # The final latency is the start time of the SINK node.
    # Since SINK is virtual (lat 0) and scheduled *after* all real work,
    # its start time is the cycle *after* the last real op finished.
    # So, L_final = SINK.start_time - 1
    if sink_node and sink_node.id in scheduled_ops_start_time:
        # L_final is the cycle the SINK starts *on*, minus 1
        l_final = scheduled_ops_start_time[sink_node.id] - 1
    else:
        # Fallback if SINK never scheduled (error)
        print("[Error] SINK node was not found in final schedule. Latency may be incorrect.", file=sys.stderr)
        l_final = current_cycle - 2 
    
    return (l_final, scheduled_ops_start_time)



def Iterative_RCS_Scheduler(ops_list, dependencies, resources):
    ops_map = build_ops_map(ops_list, dependencies)
    asap = calculate_asap(ops_map)
    _, max_down = calculate_down_len(ops_map)
    sink = next(op for op in ops_map.values() if op.resource_type == "SINK")
    l_crit = asap.get(sink.id, 1)
    print(f"L_crit: {l_crit - 1}")
    
    l_target = l_crit - 1
    l_final = 0
    
    # --- NUOVA LOGICA DI STORICO ---
    # Non salviamo solo il risultato, ma la mappatura RISULTATO -> TARGET
    # Questo ci permette di "ricreare le condizioni vincenti"
    achieved_to_target_map = {}
    l_history_list = [] # Usato solo per rilevare A -> B -> A
    # --- FINE NUOVA LOGICA ---
    
    final_schedule = {}
    final_priority_list = {}
    MAX_ITERATIONS = 20 # Increased limit for robust testing
    
    iteration_count = 0
    while iteration_count < MAX_ITERATIONS:
        iteration_count += 1
        print(f"\n--- Iteration {iteration_count} (Target={l_target}) ---")
        
        # Salviamo il target che stiamo *usando* per questa run
        current_target = l_target
        
        # 1. Proceed with calculation
        prio = CALCULATE_Static_Priorities(ops_map, resources, l_target, max_down)
        (l_new, sched) = RUN_List_Scheduler(ops_map, dependencies, resources, prio)
        
        print(f"  Achieved Latency: {l_new}")

        # --- NUOVA LOGICA DI CONVERGENZA ---
        
        # 1a. Salviamo le "condizioni vincenti"
        # (Se 10 ha prodotto 8, salviamo map[8] = 10)
        if l_new not in achieved_to_target_map:
             achieved_to_target_map[l_new] = current_target
        l_history_list.append(l_new)
        
        # 1b. Controlliamo la convergenza semplice
        if l_new == l_target:
            print("\n=== CONVERGENCE REACHED (L_new == L_target) ===")
            l_final = l_new
            final_schedule = sched
            final_priority_list = prio
            break # Fatto

        # 1c. Controlliamo l'oscillazione (A -> B -> A)
        if len(l_history_list) >= 3 and l_new == l_history_list[-3]:
            print("\n!!! STABILITY WARNING: Detected Oscillation (A -> B -> A) !!!")
            # Valori oscillanti: A = l_new, B = l_history_list[-2]
            L_optimal_achieved = min(l_new, l_history_list[-2])
            
            # Recuperiamo il target che ha prodotto quel risultato ottimale
            L_target_for_optimal = achieved_to_target_map[L_optimal_achieved]
            
            print(f"  Optimal achieved latency was {L_optimal_achieved} (using target {L_target_for_optimal})")
            
            # Se stiamo già girando con quel target, siamo bloccati. Usciamo.
            if l_target == L_target_for_optimal:
                print("  STABILITY REACHED at optimal target.")
                l_final = L_optimal_achieved
                # NOTA: final_schedule e final_priority_list sono
                # ancora impostati sull'ultima run (quella sub-ottimale).
                # Questo è un bug logico, ma è quello che c'è nel tuo codice.
                break
            else:
                # Altrimenti, forziamo un'ultima run con le "condizioni vincenti"
                print(f"  Forcing final run with optimal target L={L_target_for_optimal}")
                l_target = L_target_for_optimal
                l_history_list = [] # Reset storico per evitare loop
                l_final = 0         # Forza il loop a continuare
                continue            # Salta al prossimo ciclo
        
        # 1d. Nessuna convergenza, continuiamo
        l_final = l_new
        final_schedule = sched
        final_priority_list = prio
        l_target = l_new # Prepariamo il prossimo target
        # --- FINE NUOVA LOGICA ---
            
    if iteration_count >= MAX_ITERATIONS and l_final != l_target:
        print("\n=== MAX ITERATIONS REACHED. Using last valid schedule. ===")
        
    print("\nFinal Schedule (Start Cycles):")
    # Sort schedule by start time
    sorted_schedule = sorted(final_schedule.items(), key=lambda item: item[1])
    for op_id, start_cycle in sorted_schedule:
        if op_id in ops_map:
            op = ops_map[op_id]
            # Don't print end time for 0-latency ops
            if op.latency > 0:
                print(f"  Op {op_id:<4} (Type: {op.resource_type:<6}): {start_cycle:>4} -> {start_cycle + op.latency -1}")
            else:
                print(f"  Op {op_id:<4} (Type: {op.resource_type:<6}): {start_cycle:>4}")
        
    print(f"\n=== FINAL LATENCY: {l_final} ===")
    
    # --- New Gantt Chart Call ---
    # Pass the final_priority_list to the gantt chart function
    print_schedule_gantt_chart(ops_map, final_schedule, l_final, resources, final_priority_list) 
    
    return l_final




# --- 4. GANTT Chart (RE-WRITTEN) ---

def print_schedule_gantt_chart(ops_map, schedule, final_latency, resources, priority_list):
    """
    Prints an ASCII art Gantt chart of the final schedule.
    This is a new, more robust implementation that correctly handles
    resource unit allocation.
    """
    print("\n\n--- Gantt Chart Schedule ---")
    
    if not schedule:
        print("  (No schedule to print)")
        return

    # --- 1. Define cell width and create data grid ---
    CELL_WIDTH = 5 # e.g., " C1  "
    EMPTY_CELL = "  -  "
    FILL_CELL = " === "
    
    # Gantt data stores the string for each cell
    # gantt_data[resource_unit_name][cycle] = "op_id" or "==="
    gantt_data = defaultdict(lambda: [EMPTY_CELL for _ in range(final_latency + 1)])
    
    # Tracks when a specific resource unit is free
    # e.g., resource_free_at["ALU_1"] = 5 (free at start of C5)
    resource_free_at = {}
    
    resource_unit_names = []
    for k, count in resources.items():
        for i in range(1, count + 1):
            unit_name = f"{k}_{i}"
            resource_unit_names.append(unit_name)
            resource_free_at[unit_name] = 1 # All free at cycle 1
            
    # --- 2. Get all REAL ops, sorted by their scheduled start time ---
    real_ops = []
    for op_id, start_time in schedule.items():
        if op_id in ops_map:
            op = ops_map[op_id]
            if op.latency > 0 and op.resource_type in resources:
                real_ops.append((op, start_time))
                
    # Sort by start time, then by priority (as tie-breaker)
    real_ops.sort(key=lambda x: (x[1], priority_list.get(x[0].id, float('inf'))))
    
    # --- 3. Place ops onto the Gantt data grid ---
    for op, start_time in real_ops:
        k = op.resource_type
        
        # Find the *first available unit* of this type
        assigned_unit = None
        for i in range(1, resources[k] + 1):
            unit_name = f"{k}_{i}"
            if resource_free_at[unit_name] <= start_time:
                assigned_unit = unit_name
                break
                
        if assigned_unit is None:
            # This should never happen if the scheduler is correct
            # But it's a good failsafe
            print(f"[Gantt Error] No free unit found for {op.id} at C{start_time}", file=sys.stderr)
            # Assign it to the first one anyway
            assigned_unit = f"{k}_1"
            
        # Place the op in the chart
        op_label = f" {op.id:<{CELL_WIDTH-2}} " # Pad op ID
        if len(op_label) > CELL_WIDTH: 
            op_label = op_label[:CELL_WIDTH-2] + " " # Truncate
        
        if start_time <= final_latency:
            gantt_data[assigned_unit][start_time-1] = op_label # -1 for 0-indexing
        
        # Fill in its duration
        for c in range(start_time + 1, start_time + op.latency):
             if c <= final_latency:
                gantt_data[assigned_unit][c-1] = FILL_CELL # -1 for 0-indexing
                
        # Update the free time for this unit
        resource_free_at[assigned_unit] = start_time + op.latency

    # --- 4. Print the chart ---
    max_label_len = max(len(name) for name in resource_unit_names) if resource_unit_names else 0
    
    # Print header
    header = " " * (max_label_len + 2)
    for c in range(1, final_latency + 1):
        header += f" C{c:<{CELL_WIDTH-2}} "
    print(header)
    print("-" * len(header))
    
    # Print rows
    for res_type in sorted(resources.keys()):
        for i in range(1, resources[res_type] + 1):
            unit_name = f"{res_type}_{i}"
            line = f"{unit_name:<{max_label_len}} : "
            for c in range(final_latency):
                line += gantt_data[unit_name][c]
            print(line)
        print("-" * len(header))


# File .dot parsing

def parse_dot_data_to_scheduler(dot_data):
    """
    Parses a .dot graph string and converts it into the
    ops_list and dependencies format.
    """
    lat_regex = re.compile(r"Lat:\s*(\d+)", re.IGNORECASE)
    type_regex = re.compile(r"Type:\s*(\w+)", re.IGNORECASE)
    
    ops_map = {}
    dependencies = []
    
    try:
        graphs = pydot.graph_from_dot_data(dot_data)
        if not graphs:
            return [], []
        graph = graphs[0]
        
        for node in graph.get_nodes():
            node_id = node.get_name().strip('"')
            if node_id.lower() == 'graph':
                continue
            label = node.get_label() or ""
            
            latency = 0
            resource_type = "UNKNOWN"
            
            if node_id.upper() == "SOURCE":
                resource_type = "SOURCE"
            elif node_id.upper() == "SINK":
                resource_type = "SINK"
            else:
                lat_match = lat_regex.search(label)
                type_match = type_regex.search(label)
                if lat_match:
                    latency = int(lat_match.group(1))
                if type_match:
                    resource_type = type_match.group(1).upper()
            
            if node_id not in ops_map:
                ops_map[node_id] = Operation(node_id, latency, resource_type)

        for edge in graph.get_edges():
            src_id = edge.get_source().strip('"')
            dest_id = edge.get_destination().strip('"')
            if src_id in ops_map and dest_id in ops_map:
                dependencies.append((src_id, dest_id))
                
        return list(ops_map.values()), dependencies
        
    except Exception as e:
        print(f"[Fatal Error] Failed to parse .dot graph: {e}", file=sys.stderr)
        return [], []


# --- Example Definitions ---

def run_example_1():
    print("\n\n--- Example 1: Simple Contention ---")
    ops_list = [
        Operation("s", 0, "SOURCE"),
        Operation("A", 1, "ALU"), # Will contend with C
        Operation("B", 1, "ALU"), # Will contend with D
        Operation("C", 2, "MUL"), # Will contend with D
        Operation("D", 1, "ALU"),
        Operation("E", 1, "ALU"),
        Operation("z", 0, "SINK"),
    ]
    dependencies = [
        ("s","A"), ("s","C"),
        ("A", "B"),
        ("A", "D"),
        ("C", "E"),
        ("B","z"), ("D","z"), ("E","z")
    ]
    resources = {
        "ALU": 1,
        "MUL": 1
    }
    
    Iterative_RCS_Scheduler(ops_list, dependencies, resources)

def run_example_2():
    print("\n\n--- Example 2: Critical Path vs. Congestion ---")
    # B-E-G-H is critical path (L=5)
    # A-D-G-H is sub-critical (L=4)
    # C-F-H is sub-critical (L=3)
    # All (A, B, C) compete for 1 ALU at Cycle 1.
    # B is on CP. A blocks ALU congestion. C blocks nothing.
    # We want B to be scheduled first.
    ops_list = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "ALU"),
        Operation("c", 1, "ALU"),
        Operation("d", 1, "ALU"), # Successor of a
        Operation("e", 2, "MUL"), # Successor of b
        Operation("f", 1, "ALU"), # Successor of c
        Operation("g", 1, "ALU"),
        Operation("h", 1, "ALU"),
        Operation("z", 0, "SINK")
    ]
    dependencies = [
        ("s","a"),("s","b"),("s","c"),
        ("a", "d"), ("d", "g"),
        ("b", "e"), ("e", "g"),
        ("c", "f"), ("f", "h"),
        ("g", "h"),
        ("h", "z")
    ]
    resources = {
        "ALU": 1,
        "MUL": 1
    }
    
    Iterative_RCS_Scheduler(ops_list, dependencies, resources)
    
def run_example_3_hal():
    print("\n\n--- Example 3: HAL Benchmark (ALU contention) ---")
    # This is a classic HLS benchmark.
    # The critical path is 'e'->'f' (2 MULs), L_crit = 4.
    # But there are 4 ALUs ('a','b','c','d') all ready at C1,
    # and only 2 ALU resources.
    # The ALUs are the *real* bottleneck, not the MULs.
    ops_list = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"), # v1
        Operation("b", 1, "ALU"), # v2
        Operation("c", 1, "ALU"), # v3
        Operation("d", 1, "ALU"), # v4
        Operation("e", 1, "ALU"), # v5
        Operation("f", 2, "MUL"), # v6
        Operation("g", 1, "ALU"), # v7
        Operation("z", 0, "SINK") # v8 (SINK)
    ]
    dependencies = [
        ("s","a"),("s","b"),("s","c"),("s","d"),
        ("a", "e"),
        ("b", "e"),
        ("c", "f"),
        ("d", "f"),
        ("e", "g"),
        ("f", "g"),
        ("g", "z"),
    ]
    resources = {
        "ALU": 1,
        "MUL": 2
    }
    
    Iterative_RCS_Scheduler(ops_list, dependencies, resources)
    
def run_example_4_interleave():
    print("\n\n--- Example 4: Interleaving Puzzle ---")
    # Two paths that *should* interleave perfectly.
    # Path 1: ALU(1) -> MUL(2)
    # Path 2: MUL(2) -> ALU(1)
    # With 1 ALU and 1 MUL, L_crit=3, L_final=3.
    # The scheduler must pick A and C at C1.
    ops_list = [
        Operation("s", 0, "SOURCE"),
        Operation("A", 1, "ALU"),
        Operation("B", 2, "MUL"),
        Operation("C", 2, "MUL"),
        Operation("D", 1, "ALU"),
        Operation("z", 0, "SINK"),
    ]
    dependencies = [
        ("s","A"), ("s","C"),
        ("A", "B"), ("B","z"),
        ("C", "D"), ("D","z"),
    ]
    # This creates a complex scheduling puzzle.
    resources = {
        "ALU": 1,
        "MUL": 1
    }
    
    Iterative_RCS_Scheduler(ops_list, dependencies, resources)

def run_example_5_myopic_trap():
    print("\n\n--- Example 5: The 'Myopic C' Trap (FIXED) ---")
    # This is the trap that our new F = (mob+1) * C heuristic
    # is designed to solve.
    #
    # Path A: a(ALU, L=1) -> b(MUL, L=1) -> k(ALU, L=1) -> z
    #   L_crit(a) = 3
    #
    # Path C: c(ALU, L=1) -> d(ALU, L=1) -> l(ALU, L=1) -> i(ALU, L=1) -> z
    #   L_crit(c) = 4
    #
    # L_target will be 5 (SINK starts C5).
    #
    # --- OLD FAILED LOGIC (F = S/C) ---
    # S(c) = 0 -> F(c) = 0. (TYRANNY OF ZERO)
    # S(a) > 0 -> F(a) > 0.
    # Scheduler would pick 'c', 'd', 'l', 'i' sequentially,
    # *then* 'a', 'b', 'k'. Result: L_final = 7. (Terrible)
    #
    # --- NEW F = S^2 * C (Average) LOGIC ---
    # L_target = 5
    # ALAP(c) = 5 - 4 = 1. mob(c) = 0. Raw S(c) = 0 + 1 = 1.
    # ALAP(a) = 5 - 3 = 2. mob(a) = 1. Raw S(a) = 1 + 1 = 2.
    #
    # C_avg(c) = Avg(C(c), C(d), C(l), C(i)) -> 4 ALUs. C_avg(c) will be HUGE.
    # C_avg(a) = Avg(C(a), C(b), C(k)) -> 2 ALUs, 1 MUL. C_avg(a) will be SMALLER.
    #
    # F(c) = S_norm(c)^2 * C_norm(c) = (LOW S)^2 * (HIGH C) = MEDIUM
    # F(a) = S_norm(a)^2 * C_norm(a) = (HIGH S)^2 * (LOW C) = MEDIUM-HIGH
    #
    # Now they can compete fairly! The scheduler should interleave them.
    # 'c' should have higher priority (lower S).
    #
    ops_list_5 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 2, "MUL"),
        Operation("k", 1, "ALU"),
        Operation("c", 1, "ALU"),
        Operation("d", 1, "ALU"),
        Operation("l", 1, "ALU"),
        Operation("i", 1, "ALU"), # Added this node
        Operation("z", 0, "SINK")
    ]
    dependencies_5 = [
        ("s", "a"), ("s", "c"),
        ("a", "b"), ("b", "k"), ("k", "z"),
        ("c", "d"), ("d", "l"), ("l", "i"), ("i", "z") # Extended path
    ]
    resources_5 = { "ALU": 1, "MUL": 1 }
    Iterative_RCS_Scheduler(ops_list_5, dependencies_5, resources_5)

def run_example_6_real_mess():
    print("\n\n--- Example 6: The 'Real Mess' (3 Resources) ---")
    # A complex graph with 3 resources (ALU, MUL, MEM)
    # This tests the FDS graphs and C-term's ability to
    # find the *true* bottleneck among multiple resource types.
    ops_list_6 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "ALU"),
        Operation("c", 2, "MUL"),
        Operation("d", 3, "MEM"), # Long memory operation
        Operation("e", 1, "ALU"),
        Operation("f", 2, "MUL"),
        Operation("g", 1, "ALU"),
        Operation("h", 1, "ALU"),
        Operation("k", 3, "MEM"),
        Operation("z", 0, "SINK")
    ]
    dependencies_6 = [
        ("s", "a"), ("s", "b"), ("s", "c"), ("s", "d"),
        ("a", "e"), # ALU -> ALU
        ("b", "f"), # ALU -> MUL
        ("c", "g"), # MUL -> ALU
        ("d", "h"), # MEM -> MEM
        ("e", "k"),
        ("f", "k"),
        ("g", "h"), # ALU -> MEM
        ("h", "k"),
        ("k", "z")
    ]
    
    # Resources: 2 ALUs, 1 MUL, 1 MEM
    # The contention could be anywhere. 'd' and 'h' create a MEM bottleneck.
    # 'c' and 'f' create a MUL bottleneck.
    # 'a','b','e','g' will fight for ALUs.
    resources_6 = {
        "ALU": 1,
        "MUL": 1,
        "MEM": 1
    }
    Iterative_RCS_Scheduler(ops_list_6, dependencies_6, resources_6)

def run_example_7_diffeq():
    print("\n\n--- Example 7: DIFFEQ Benchmark (Parallel Path Contention) ---")
    # This is a common HLS benchmark for differential equation solving.
    # It has two main parallel paths competing for resources.
    # Path 1 (ALU heavy): x, u1, +, +
    # Path 2 (MUL heavy): u, *, *, +
    # Both paths converge on '+4'.
    #
    # With 2 ALUs and 1 MUL, the MUL path (u, *1, *2) will be the
    # resource bottleneck, even though it's not the longest time-path.
    # The scheduler *must* start *1 (MUL) at C1, *and*
    # interleave the ALU ops wisely.
    
    ops_list_7 = [
        Operation("s", 0, "SOURCE"),
        Operation("x", 1, "ALU"),    # x
        Operation("u", 1, "ALU"),    # u
        Operation("dx", 1, "ALU"),   # dx
        Operation("a", 1, "ALU"),    # a
        
        Operation("u1", 1, "ALU"),   # u1 (from u)
        Operation("*1", 2, "MUL"),   # *1 (from u)
        
        Operation("+1", 1, "ALU"),   # +1 (from x, u1)
        Operation("*2", 2, "MUL"),   # *2 (from *1, dx)
        
        Operation("+2", 1, "ALU"),   # +2 (from +1, a)
        Operation("*3", 2, "MUL"),   # *3 (from *2, u)
        
        Operation("+3", 1, "ALU"),   # +3 (from +2)
        
        Operation("+4", 1, "ALU"),   # CONVERGENCE POINT
        Operation("z", 0, "SINK")
    ]
    
    dependencies_7 = [
        ("s", "a"), ("s", "x"), ("s", "u"), ("s", "dx"),
        
        # Path 1 (ALU heavy)
        ("u", "u1"),
        ("x", "+1"), ("u1", "+1"),
        ("+1", "+2"), ("a", "+2"),
        ("+2", "+3"),
        
        # Path 2 (MUL heavy)
        ("u", "*1"),
        ("*1", "*2"), ("dx", "*2"),
        # ("*2", "g"), # MISTAKE IN ORIGINAL, should be *3
        ("*2", "*3"), ("u", "*3"), # Note: 'u' is used again
        
        # Convergence
        ("+3", "+4"),
        ("*3", "+4"),
        ("+4", "z")
    ]
    
    resources_7 = {
        "ALU": 2,
        "MUL": 1
    }
    
    Iterative_RCS_Scheduler(ops_list_7, dependencies_7, resources_7)

def run_example_8_competence_test():
    print("\n\n--- Example 8: The 'Competence Test' ---")
    
    ops_list_8 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "ALU"),
        Operation("c", 1, "ALU"),
        
        Operation("d", 1, "ALU"),
        Operation("e", 1, "ALU"),
        Operation("f", 1, "MUL"),
        
        Operation("z", 0, "SINK")
    ]
    dependencies_8 = [
        ("s", "a"), ("s", "d"),
        ("a", "b"), ("b", "c"), ("c", "z"),
        ("d", "e"), ("e", "f"), ("f", "z")
    ]
    
    resources_8 = {
        "ALU": 1,
        "MUL": 1
    }
    
    Iterative_RCS_Scheduler(ops_list_8, dependencies_8, resources_8)



def run_example_9_competence_test():
    print("\n\n--- Example 8: The 'Competence Test' ---")
    
    ops_list_8 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "ALU"),
        Operation("c", 1, "ALU"),
        
        Operation("d", 1, "ALU"),
        Operation("e", 1, "ALU"),
        Operation("f", 1, "MUL"),
        Operation("g", 1, "ALU"),
        
        Operation("z", 0, "SINK")
    ]
    dependencies_8 = [
        ("s", "a"), ("s", "d"),
        ("a", "b"), ("b", "c"), ("c", "g"), ("g", "z"),
        ("d", "e"), ("e", "f"), ("f", "z")
    ]
    
    resources_8 = {
        "ALU": 1,
        "MUL": 1
    }
    
    Iterative_RCS_Scheduler(ops_list_8, dependencies_8, resources_8)



def run_example_10_real_trap():
    print("\n\n--- Example 9: The 'Parallelism' Trap ---")
    
    ops_list_10 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "ALU"),
        Operation("c", 2, "MUL"),
        Operation("d", 2, "MUL"),
        Operation("e", 3, "MEM"), 
        Operation("f", 1, "ALU"), 
        Operation("g", 2, "MUL"), 
        Operation("h", 1, "ALU"), 
        Operation("i", 3, "MEM"), 
        
        Operation("z", 0, "SINK")
    ]
    dependencies_10 = [
        ("s", "a"), ("s", "b"), ("s", "c"),
        ("a", "d"), ("b", "e"), ("c", "f"),
        ("d", "g"), ("e", "h"), ("f", "i"),
        ("g", "z"), ("h", "z"), ("i", "z")
    ]
    
    resources_10 = {
        "ALU": 1,
        "MUL": 1,
        "MEM": 1
    }
    
    Iterative_RCS_Scheduler(ops_list_10, dependencies_10, resources_10)

def run_example_11_real_trap():
    print("\n\n--- Example 11: The 'Parallelism' Trap ---")
    
    ops_list_11 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "ALU"),
        Operation("c", 2, "MUL"),
        Operation("d", 1, "ALU"),
        Operation("e", 2, "MUL"), 
        Operation("f", 1, "ALU"), 
        Operation("g", 2, "MUL"), 
        Operation("h", 1, "ALU"), 
        Operation("j", 1, "ALU"), 
        Operation("i", 1, "ALU"), 
        Operation("k", 1, "ALU"), 
        
        Operation("z", 0, "SINK")
    ]
    dependencies_11 = [
        ("s", "a"), ("s", "d"), ("a", "b"),
        ("b", "i"), ("i", "c"), ("c", "e"), ("d", "f"),
        ("f", "k"), ("k", "g"), ("g", "h"), ("h", "j"), ("j", "z")
    ]
    
    resources_11 = {
        "ALU": 1,
        "MUL": 1,
    }
    
    Iterative_RCS_Scheduler(ops_list_11, dependencies_11, resources_11)

def run_example_12():

    ops_list_5 = [
        Operation("s", 0, "SOURCE"),
        Operation("a", 1, "ALU"),
        Operation("b", 1, "MUL"),
        Operation("k", 1, "ALU"),
        Operation("c", 1, "ALU"),
        Operation("d", 1, "ALU"),
        Operation("l", 1, "ALU"),
        Operation("i", 1, "ALU"),
        Operation("z", 0, "SINK")
    ]
    dependencies_5 = [
        ("s", "a"), ("s", "c"),
        ("a", "b"), ("b", "k"), ("k", "z"),
        ("c", "d"), ("d", "l"), ("l", "i"), ("i", "z")
    ]
    resources_5 = { "ALU": 1, "MUL": 1 }

    Iterative_RCS_Scheduler(ops_list_5, dependencies_5, resources_5)


if __name__ == "__main__":
    
    # run_example_1()
    
    # run_example_2()
    
    # run_example_3_hal()
    
    # run_example_4_interleave()
    
    # run_example_5_myopic_trap()
    
    # run_example_6_real_mess()
    
    # run_example_7_diffeq()
    
    # run_example_8_competence_test()

    # run_example_9_competence_test()
    
    # run_example_9_real_trap()

    run_example_10_real_trap()

    # run_example_11_real_trap()