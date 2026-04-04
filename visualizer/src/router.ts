import { createRouter, createWebHistory } from "vue-router";
import OverviewPage from "./views/OverviewPage.vue";
import Step3Page from "./views/Step3Page.vue";
import Step4Page from "./views/Step4Page.vue";
import Step5Page from "./views/Step5Page.vue";
import Step6Page from "./views/Step6Page.vue";
import Step7Page from "./views/Step7Page.vue";
import Step8Page from "./views/Step8Page.vue";

export const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: "/", redirect: "/overview" },
    { path: "/overview", component: OverviewPage },
    { path: "/step3", component: Step3Page },
    { path: "/step4", component: Step4Page },
    { path: "/step5", component: Step5Page },
    { path: "/step6", component: Step6Page },
    { path: "/step7", component: Step7Page },
    { path: "/step8", component: Step8Page }
  ]
});
