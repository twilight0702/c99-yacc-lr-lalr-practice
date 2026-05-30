import { defineStore } from "pinia";
import type { CaseIndexItem, Manifest, StepData } from "../types";
import { loadCaseIndex, loadManifest, loadStepData, resolveInitialCaseId, sortCasesDesc } from "../utils/data";

interface YaccState {
  loading: boolean;
  casesLoading: boolean;
  error: string;
  casesError: string;
  currentCaseId: string;
  latestCaseId: string;
  cases: CaseIndexItem[];
  manifest: Manifest | null;
  stepData: Record<number, StepData>;
}

export const useYaccStore = defineStore("yacc", {
  state: (): YaccState => ({
    loading: false,
    casesLoading: false,
    error: "",
    casesError: "",
    currentCaseId: "",
    latestCaseId: "",
    cases: [],
    manifest: null,
    stepData: {}
  }),
  getters: {
    currentCase(state): CaseIndexItem | null {
      return state.cases.find((item) => item.case_id === state.currentCaseId) ?? null;
    }
  },
  actions: {
    invalidateCases(): void {
      this.cases = [];
      this.latestCaseId = "";
      this.casesError = "";
    },
    resetCaseData(): void {
      this.error = "";
      this.manifest = null;
      this.stepData = {};
    },
    async ensureCases(): Promise<void> {
      if (this.cases.length) {
        return;
      }
      this.casesLoading = true;
      this.casesError = "";
      try {
        const index = await loadCaseIndex();
        this.cases = sortCasesDesc(index.cases ?? []);
        this.latestCaseId = index.latest_case_id || this.cases[0]?.case_id || "";
      } catch (err) {
        this.casesError = err instanceof Error ? err.message : String(err);
      } finally {
        this.casesLoading = false;
      }
    },
    async ensureCurrentCase(caseIdHint?: string): Promise<void> {
      await this.ensureCases();
      const fallbackCaseId = this.latestCaseId || (await resolveInitialCaseId());
      const requestedCaseId = caseIdHint?.trim() || "";
      if (!requestedCaseId && this.currentCaseId) {
        return;
      }
      const isKnownCase = !requestedCaseId || !this.cases.length || this.cases.some((item) => item.case_id === requestedCaseId);
      const nextCaseId = isKnownCase ? requestedCaseId || fallbackCaseId : fallbackCaseId;
      if (!this.currentCaseId) {
        this.currentCaseId = nextCaseId;
        return;
      }
      if (this.currentCaseId !== nextCaseId) {
        this.currentCaseId = nextCaseId;
        this.resetCaseData();
      }
    },
    async ensureManifest(): Promise<void> {
      await this.ensureCurrentCase();
      if (this.manifest) {
        return;
      }
      this.loading = true;
      this.error = "";
      try {
        this.manifest = await loadManifest(this.currentCaseId);
      } catch (err) {
        this.error = err instanceof Error ? err.message : String(err);
      } finally {
        this.loading = false;
      }
    },
    async ensureStep(step: number): Promise<void> {
      await this.ensureManifest();
      if (!this.manifest || this.stepData[step]) {
        return;
      }
      this.loading = true;
      this.error = "";
      try {
        this.stepData[step] = await loadStepData(this.manifest, step);
      } catch (err) {
        this.error = err instanceof Error ? err.message : String(err);
      } finally {
        this.loading = false;
      }
    }
  }
});
