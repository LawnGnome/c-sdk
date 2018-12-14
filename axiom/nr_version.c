#include "nr_axiom.h"
#include "nr_version.h"

/*
 * NR_VERSION ultimately comes from the top-level VERSION file.
 */
#ifndef NR_VERSION
#define NR_VERSION "unreleased"
#endif

/*
 * NR_COMMIT ultimately comes from the command $(git rev-parse HEAD)
 */
#ifndef NR_COMMIT
#define NR_COMMIT ""
#endif

/*
 * The Release code name, assigned by whimsy, referring to a dinosaur,
 * progressing alphabetically.
 * See http://en.wikipedia.org/wiki/List_of_dinosaurs
 *
 * Please update this comment with the name you choose
 * and the date you chose it.
 *
 *   hadrosaurus             05Jul2013 (3.7)
 *   iguanodon               12Jul2013 (3.8)
 *   juratyrant              13Aug2013 (3.9)
 *   khaan                   03Sep2013 (4.0) (rolled from 3.10 to 4.0 on
 * 17Sep2013 because of PHP5.1 out and PHP5.5 in) lexovisaurus
 * 14Oct2013 (4.1) micropachycephalosaurus 07Nov2013 (4.2) (branch taken on
 * 07Nov2013) nqwebasaurus            25Nov2013 (4.3) (branch taken on
 * 25Nov2013) (Adam named this one) ozraptor                29Dec2013 (4.4)
 * (branch taken on 29Dec2013) (Adam named this one) pyroraptor
 * 31Jan2014 (4.5) (branch taken on 31Jan2014) (Mike named this one)
 *   quetzalcoatlus          24Feb2014 (4.6) (branch taken on 24Feb2014)
 * (Robert's niece named, studying dinosaurs, named this one) richardoestesia
 * 24Feb2014 (4.7) (branch taken on 25Feb2014) (Richard England named this
 * one) spinops                 21Apr2014 (4.8) (branch taken on 20Mar2014)
 * (Rich Vanderwal named this one) trex                    15May2014 (4.9)
 * (branch taken on 15May2014) (Common consent named this one) unenlagia
 * 18May2014 (4.10) (branch taken on 18Jun2014) (Robert named this one)
 *   vandersaur              18Jun2014 (4.11) (Rich named this one) (branch
 * taken on 23Jul2014) wannanosaurus           23Jul2014 (4.12) (Robert named
 * this one) (branch taken on 23Jul2014) xiaotingia              29Aug2014
 * (4.13) (Aaron named this one) yulong                  10Sep2014 (4.14)
 * (Adam named this one) (branch taken on 22Sep2014) zanabazar
 * 22Sep2014 (4.15) (Rich named this one) (branch taken on 22Oct2014)
 *
 * On 22Oct2014 we switched to the naming scheme based on bird genera.
 * Here's an exhaustive list:
 *   http://en.wikipedia.org/wiki/List_of_bird_genera
 *
 * There's no wikipedia enumeration of just North America birds,
 * but these lists may prove useful:
 *   https://www.pwrc.usgs.gov/library/bna/bnatbl.htm
 *   http://checklist.aou.org/taxa/
 *
 *   aquila                  22Oct2014 (4.16) (Robert named this one)
 *   barnardius              25Nov2014 (4.17) (Adam named this one after parrots
 * he grew up with) (branch taken on 16Dec2014) corvid
 * 17Dec2014 (4.18) (Walden 2nd Robert's suggestion) drepanis
 * 20Jan2015 (4.19) (Will named this one) emberiza                26Feb2015
 * (4.20) (Adam named this one) fregata                 27Mar2015 (4.21)
 * (Galen named this one) gallus                  23Apr2015 (4.22) (Adam named
 * this one) hydrobatidae            27May2015 (4.23) (Robert named this one)
 *   ispidina                01Jul2015 (4.24) (Erika named this one)
 *   jacana                  14Oct2015 (5.0)  (Will named this one)
 *
 * For release 5.1, we switched to a naming scheme based on women
 * mathematicians. Feel free to use either the first or last name. Here's a list
 * to use: https://en.wikipedia.org/wiki/List_of_women_in_mathematics
 *
 *   ada                     28Oct2015 (5.1) (Chris named this one)
 *   billey                  14Dec2015 (5.2) (Mike named this one)
 *   chudnovsky              19Jan2016 (5.3) (Rich named this one)
 *   driscoll                26Jan2016 (5.4) (Adam named this one)
 *   easley                  08Feb2016 (5.5) (Erika named this one)
 *   freitag                 24Feb2016 (6.0) (Tyler named this one)
 *   gordon                  14Mar2016 (6.1) (Will named this one)
 *   harizanov               22Mar2016 (6.2) (Will named this one)
 *   iyengar                 11Apr2016 (6.3) (Tyler, Rich, and Erika named this
 * one) jitomirskaya            11May2016 (6.4) (Tyler) krieger
 * 22Jun2016 (6.5) (Tyler) lacampagne              25Jul2016 (6.6) (Mike)
 *   maslennikova            23Aug2016 (6.7) (Aidan)
 *   noether                 20Sep2016 (6.8) (Erika)
 *   owens                   13Dec2016 (6.9) (Rich)
 *   pipher                  12Jan2017 (7.0) (Erika)
 *   roth                    14Feb2017 (7.1) (Adam)
 *   senechal                15Mar2017 (7.2) (Chris)
 *   tjoetta                 19Apr2017 (7.3) (Rich)
 *   uhlenbeck               26Jun2017 (7.4) (Erika)
 *   vaughan                 05Jul2017 (7.5) (Tanya)
 *   weber                   06Sep2017 (7.6) (Erika)
 *   yershova                20Nov2017 (7.7) (Tanya)
 *   zahedi                  25Jan2018 (7.8) (Tanya)
 */
#define NR_CODENAME "zahedi"

const char* nr_version(void) {
  return NR_STR2(NR_VERSION);
}

const char* nr_version_verbose(void) {
  return NR_STR2(NR_VERSION) " (\"" NR_CODENAME
                             "\" - \"" NR_STR2(NR_COMMIT) "\")";
}
