#include "analyze.hpp"
#include "cpp-revca.hpp"
#include "field.hpp"
#include "random_search.hpp"
#include "mathutil.hpp"
#include "tree_pattern.hpp"

#include <vector>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <tuple>
#include <stdexcept>
#include <cstdlib>

using namespace std;


double pattern_energy( const Pattern &p )
{
    using namespace std;
    int n = (int)p.size();
    double e = 0;
    for (int i = 0; i < n; ++i) {
      const Cell & c1 = p.points[i];
      for (int j = i+1; j < n; ++j) {
	Cell d = p.points[j] - c1;
        e += 1.0 / sqrt(sqrt(d[0]*d[0]+d[1]*d[1]));
      }
    }
    
    auto bounds = p.bounds();
    Cell size = get<1>(bounds) - get<0>(bounds);
    return e / ((size[0] + 1) * (size[1] + 1));
}

void analyze(const Pattern &pattern_, const MargolusBinaryRule &rule, 
	     const AnalyzeOpts &options,
	     AnalysysResult &result) {
  Analyzer analyzer(rule);
  (AnalyzeOpts&)(analyzer) = options; //write options inside
  result = analyzer.process( pattern_ );
}


const Transform rotations[4] =
  { Transform( 1,0,0,1 ),
    Transform( 0,-1,1,0),
    Transform( -1,0,0,-1),
    Transform( 0,1,-1,0) };

const Transform & normalizing_rotation( const Cell &offset )
{
  if (offset==Cell(0,0)) return rotations[0];

  for( int i=0; i<4; ++i){
    Cell offset1 = rotations[i](offset);
    if (offset1[0] > 0 && offset1[1] >= 0)
      return rotations[i];
  }
  throw logic_error("impossible situation");
}


AnalyzerCache::AnalyzerCache()
{}

size_t AnalyzerCache::put( const AnalysysResult &result )
{
  unique_ptr<AnalysysResult> presult(new AnalysysResult(result));
  results.push_back(move( presult ) );
  return results.size()-1;
}

AnalysysResult *AnalyzerCache::get_cached( const Pattern &p )
{
  auto found = cache.find(p);
  if (found != cache.end())
    return found -> second;
  else
    return nullptr;
}
void AnalyzerCache::put( const Pattern &key, size_t result_index )
{
  cache[key] = results[result_index].get();
}


struct EnergyFunc{
  double operator ()(const std::pair<Pattern, int> &p_phase)const{
    return pattern_energy(p_phase.first); 
  };
};
AnalysysResult Analyzer::process( const Pattern &pattern_)
{
  AnalysysResult result;

  MargolusBinaryRule stable_rules[] = {rule};

  int vacuum_period = 1;//stable_rules.length;

  int phase = 0;

  on_start_processing( pattern_ );
  Pattern pattern(pattern_);
  
  pattern.normalize();

  Pattern curPattern(pattern);

  result. analyzed_generations = max_iters;
  result. resolution = AnalysysResult::ITERATIONS_EXCEEDED;
  result.period = -1;

  Cell offset;
  for (int iter = vacuum_period; iter <= max_iters; iter += vacuum_period) {
    for (int irule=0;irule<vacuum_period;++irule) {
      evaluateCellList(stable_rules[irule], curPattern, phase, curPattern);
      phase ^= 1;
    }
    curPattern.sort();
    on_iteration( iter, curPattern );
    if (isOffsetEqualWithOddity( pattern, curPattern, odd(phase), offset)){
      //cycle found!
      result.resolution = AnalysysResult::CYCLE_FOUND;
      result.period = iter;
      result.offset = offset;
      on_result_found( pattern_, result );
      return result;
    }
    if (curPattern.size() > (size_t)max_population) {
      result.resolution = AnalysysResult::PATTERN_TOO_BIG;
      break;
    }
    auto bounds = pattern.bounds();
    Cell size = get<1>(bounds) - get<0>(bounds);
    if (max(abs(size[0]), abs(size[1])) > max_size){
      result.resolution = AnalysysResult::PATTERN_TO_WIDE;
      break;
    }
  }
  //search for cycle finished
  result.offset = Cell(0,0);
  on_result_found( pattern_, result );
  return result;
}

void CachingAnalyzer::on_start_processing( const Pattern &pattern )
{
  using namespace std;
  if (cache_frozen) return;
  evolution.clear();
  unique_ptr<Pattern> ppattern(new Pattern(pattern));
  evolution.push_back(move(ppattern));
}

void CachingAnalyzer::on_iteration( int age, const Pattern &pattern )
{
  using namespace std;
  if (cache_frozen) return;
  unique_ptr<Pattern> ppattern(new Pattern(pattern));
  evolution.push_back(move(ppattern));
}

void CachingAnalyzer::on_result_found( const Pattern &pattern, const AnalysysResult &result)
{
  using namespace std;
  if (cache_frozen) return;
  if (result.resolution == AnalysysResult::CYCLE_FOUND){
    size_t result_index = cache.put( result );
    for( unique_ptr<Pattern> &ppattern: evolution ){
      //TODO: actually, all possible rotations must be put!
      Pattern p(*ppattern);
      p.normalize();
      cache.put( p, result_index );
    }
  }
  evolution.clear();
}

AnalysysResult CachingAnalyzer::process( const Pattern &pattern)
{
  using namespace std;
  Pattern normalized_pattern(pattern);
  normalized_pattern.normalize();
  AnalysysResult * presult = cache.get_cached(normalized_pattern);
  
  if( presult != nullptr ){
    cache_hits ++;
    return *presult;
  }else{
    cache_misses ++;
    return Analyzer::process( pattern );
  }
}

struct TreePatternEnergy{
  double operator()( const TreePattern &p ){
    Pattern temp;
    p.to_list(temp);
    return pattern_energy(temp);
  };
};

/**Search for the most compact form (minimizing energy) of the pattern*/
Pattern most_compact_form( const Pattern &p, size_t period, const MargolusBinaryRule &rule )
{
  AnalysysResult result;
  Maximizer<pair<Pattern, int>, EnergyFunc, double> bestPatternSearch;

  MargolusBinaryRule stable_rules[] = {rule};

  int vacuum_period = 1;//stable_rules.length;

  int phase = 0;
  Pattern curPattern(p);
  bestPatternSearch.put(make_pair(curPattern, phase) ); //initial phase is 0
  for (size_t iter = vacuum_period; iter <= period; iter += vacuum_period) {
    for (int irule=0;irule<vacuum_period;++irule) {
      evaluateCellList(stable_rules[irule], curPattern, phase, curPattern);
      phase ^= 1;
    }
    bestPatternSearch.put(make_pair(curPattern, phase));
  }
  //search for cycle finished
  int bestValuePhase = bestPatternSearch.getBestValue().second;
  Pattern bestPattern = bestPatternSearch.getBestValue().first;
  bestPattern.translate(bestValuePhase,bestValuePhase);
  bestPattern.normalize();
  return bestPattern;
}

AnalysysResult analyze_with_trees( const TreePattern &pattern, const MargolusBinaryRule &rule, int max_iters, int max_population, int max_size)
{
  AnalysysResult result;

  MargolusBinaryRule stable_rules[] = {rule};

  int vacuum_period = 1;//stable_rules.length;

  int phase = 0;

  TreePattern cur_pattern(pattern);

  result.analyzed_generations = max_iters;
  result.resolution = AnalysysResult::ITERATIONS_EXCEEDED;
  result.period = -1;
  result.max_size = 0;

  Cell block_offset;
  for (int iter = vacuum_period; iter <= max_iters; iter += vacuum_period) {
    for (int irule=0;irule<vacuum_period;++irule) {
      TreePattern _nextStep;
      cur_pattern.evaluate(stable_rules[irule], phase, _nextStep);
      cur_pattern.swap(_nextStep);
      phase ^= 1;
    }
    if (pattern.shift_equal(cur_pattern, block_offset)){
      //cycle found!
      result.offset = block_offset*2;
      if (phase == 1)
	result.offset += Cell(-1,-1);
      result.resolution = AnalysysResult::CYCLE_FOUND;
      result.period = iter;
      return result;
    }
    if (cur_pattern.blocks_size() > (size_t)max_population) {
      result.resolution = AnalysysResult::PATTERN_TOO_BIG;
      break;
    }
    
    pair<Cell, Cell> bounds = cur_pattern.block_bounds();
    Cell bsize = bounds.second - bounds.first;
    int size = max(bsize[0], bsize[1])*2+2;
    result.max_size = (max)(result.max_size, size);
    if (size > max_size){
      result.resolution = AnalysysResult::PATTERN_TO_WIDE;
      break;
    }
  }
  //search for cycle finished
  result.offset = Cell(0,0);
  return result;
}

AnalysysResult TreeAnalyzer::process( const Pattern &pattern)
{
  TreePattern tpattern;
  tpattern.from_list(pattern);
  return analyze_with_trees( tpattern, rule, max_iters, max_population, max_size );
}
