const fs = require('fs');
const path = require('path');
const config = require('../config');

const CAUSES_FILE = path.join(__dirname, '../..', config.causesFile);

const DEFAULT_CAUSES = [
  'تصنيع - تصنيع و فحص',
  'جودة - رفض خامات',
  'جودة - عينة اولية',
  'جودة - منتج',
  'خامات - تخطيط - PLAN',
  'خامات - شرينك',
  'خامات - عبوات',
  'خامات - غطاء',
  'خامات - كرتون',
  'خامات - ليبول',
  'صيانة - ماكينة التعبئة',
  'صيانة - ماكينة الشرينك',
  'صيانة - ماكينة الطبة',
  'صيانة - ماكينة الغطاء',
  'صيانة - ماكينة الكرتون',
  'صيانة - ماكينة الليبول',
  'ماكينة - adj',
  'ماكينة - غسيل',
  'ماكينة - غسيل + adj'
];

const DEFAULT_LOCATIONS = [
  'Filling Machine',
  'Shrink Machine',
  'Cap Machine',
  'Label Machine',
  'Carton Machine',
  'Production Line',
  'Quality Lab',
  'Warehouse'
];

function loadCauses() {
  if (!fs.existsSync(CAUSES_FILE)) {
    const defaults = { causes: DEFAULT_CAUSES, locations: DEFAULT_LOCATIONS };
    fs.writeFileSync(CAUSES_FILE, JSON.stringify(defaults, null, 2));
    return defaults;
  }
  return JSON.parse(fs.readFileSync(CAUSES_FILE, 'utf8'));
}

function saveCauses(causes) {
  fs.writeFileSync(CAUSES_FILE, JSON.stringify(causes, null, 2));
}

module.exports = { loadCauses, saveCauses, CAUSES_FILE };
