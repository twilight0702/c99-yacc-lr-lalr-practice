export interface ManifestStepInfo {
  step: number;
  title: string;
  files: Record<string, string>;
}

export interface Manifest {
  schema_version: string;
  case_id: string;
  generated_at: string;
  source: string;
  steps: Record<string, ManifestStepInfo>;
}

export interface SymbolRow {
  id: number;
  name: string;
  kind: string;
  is_literal_char: boolean;
}

export interface ProductionRow {
  id: number;
  lhs: string;
  rhs: string[];
  line: number;
  action: boolean;
  text: string;
}

export interface StepData {
  step: number;
  source: string;
  summary: Record<string, string>;
  analysis: Record<string, string>;
  symbols?: SymbolRow[];
  productions?: ProductionRow[];
  augmented?: { production: string };
  prod_index_by_lhs?: Array<{
    lhs_id: number;
    lhs_name: string;
    production_ids: number[];
  }>;
  first_sets?: Array<{
    symbol_id: number;
    symbol_name: string;
    kind: string;
    first_set: string[];
  }>;
  lr1_i0?: {
    kernel_items: string[];
    closure_items: string[];
    goto_edges: Array<{
      symbol_id: number;
      symbol_name: string;
      item_count: number;
    }>;
    goto_items: Record<string, string[]>;
    lookahead_notes: string[];
  };
  lr1_canonical?: {
    states: Array<{
      state_id: number;
      item_count: number;
    }>;
    state_items: Record<string, string[]>;
    transitions: Array<{
      from_state: number;
      symbol_id: number;
      symbol_name: string;
      to_state: number;
    }>;
    predecessors: Array<{
      state_id: number;
      predecessors: string[];
    }>;
  };
  parse_table?: {
    action_rows: Array<{
      state_id: number;
      terminal_id: number;
      terminal_name: string;
      action: string;
      target: string;
    }>;
    goto_rows: Array<{
      state_id: number;
      nonterminal_id: number;
      nonterminal_name: string;
      to_state: number;
    }>;
    conflicts: Array<{
      state_id: number;
      symbol_id: number;
      symbol_name: string;
      conflict_type: string;
      existing_action: string;
      incoming_action: string;
      related_items: string;
    }>;
    conflict_resolutions: Array<{
      state_id: number;
      symbol_id: number;
      symbol_name: string;
      conflict_type: string;
      existing_action: string;
      incoming_action: string;
      resolved_action: string;
      reason: string;
    }>;
  };
}
